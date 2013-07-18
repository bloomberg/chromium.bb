// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CancelableRequestProviders and Consumers work together to make requests that
// execute on a background thread in the provider and return data to the
// consumer. These classes collaborate to keep a list of open requests and to
// make sure that requests do not outlive either of the objects involved in the
// transaction.
//
// If you do not need to return data to the consumer, do not use this system,
// just use the regular Task/RunnableMethod stuff.
//
// The CancelableRequest object is used internally to each provider to track
// request data and callback information.
//
// Example consumer calling |StartRequest| on a frontend service:
//
//   class MyClass {
//     void MakeRequest() {
//       frontend_service->StartRequest(
//           some_input1, some_input2,
//           &callback_consumer_,
//           base::Bind(&MyClass:RequestComplete, base::Unretained(this)));
//       // StartRequest() returns a Handle which may be retained for use with
//       // CancelRequest() if required, e.g. in MyClass's destructor.
//     }
//
//     void RequestComplete(int status) {
//       ...
//     }
//
//    private:
//     CancelableRequestConsumer callback_consumer_;
//   };
//
//
// Example frontend provider. It receives requests and forwards them to the
// backend on another thread:
//
//   class Frontend : public CancelableRequestProvider {
//     typedef Callback1<int>::Type RequestCallbackType;
//
//     Handle StartRequest(int some_input1, int some_input2,
//                         CancelableRequestConsumerBase* consumer,
//                         RequestCallbackType* callback) {
//       scoped_refptr<CancelableRequest<RequestCallbackType> > request(
//           new CancelableRequest<RequestCallbackType>(callback));
//       AddRequest(request, consumer);
//
//       // Send the parameters and the request to the backend thread.
//       backend_thread_->PostTask(
//           FROM_HERE,
//           base::Bind(&Backend::DoRequest, backend_, request,
//                             some_input1, some_input2));
//
//       // The handle will have been set by AddRequest.
//       return request->handle();
//     }
//   };
//
//
// Example backend provider that does work and dispatches the callback back
// to the original thread. Note that we need to pass it as a scoped_refptr so
// that the object will be kept alive if the request is canceled (releasing
// the provider's reference to it).
//
//   class Backend {
//     void DoRequest(
//         scoped_refptr< CancelableRequest<Frontend::RequestCallbackType> >
//             request,
//         int some_input1, int some_input2) {
//       if (request->canceled())
//         return;
//
//       ... do your processing ...
//
//       // Depending on your typedefs, one of these two forms will be more
//       // convenient:
//       request->ForwardResult(Tuple1<int>(return_value));
//
//       // -- or --  (inferior in this case)
//       request->ForwardResult(Frontend::RequestCallbackType::TupleType(
//           return_value));
//     }
//   };

#ifndef CHROME_BROWSER_COMMON_CANCELABLE_REQUEST_H_
#define CHROME_BROWSER_COMMON_CANCELABLE_REQUEST_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_internal.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"

class CancelableRequestBase;
class CancelableRequestConsumerBase;

// CancelableRequestProvider --------------------------------------------------
//
// This class is threadsafe. Requests may be added or canceled from any thread,
// but a task must only be canceled from the same thread it was initially run
// on.
//
// It is intended that providers inherit from this class to provide the
// necessary functionality.

class CancelableRequestProvider {
 public:
  // Identifies a specific request from this provider.
  typedef int Handle;

  CancelableRequestProvider();

  // Called by the enduser of the request to cancel it. This MUST be called on
  // the same thread that originally issued the request (which is also the same
  // thread that would have received the callback if it was not canceled).
  // handle must be for a valid pending (not yet complete or cancelled) request.
  void CancelRequest(Handle handle);

 protected:
  virtual ~CancelableRequestProvider();

  // Adds a new request and initializes it. This is called by a derived class
  // to add a new request. The request's Init() will be called (which is why
  // the consumer is required. The handle to the new request is returned.
  Handle AddRequest(CancelableRequestBase* request,
                    CancelableRequestConsumerBase* consumer);

  // Called by the CancelableRequest when the request has executed. It will
  // be removed from the list of pending requests (as opposed to canceling,
  // which will also set some state on the request).
  void RequestCompleted(Handle handle);

 private:
  typedef std::map<Handle, scoped_refptr<CancelableRequestBase> >
      CancelableRequestMap;

  // Only call this when you already have acquired pending_request_lock_.
  void CancelRequestLocked(const CancelableRequestMap::iterator& item);

  friend class CancelableRequestBase;

  base::Lock pending_request_lock_;

  // Lists all outstanding requests. Protected by the |lock_|.
  CancelableRequestMap pending_requests_;

  // The next handle value we will return. Protected by the |lock_|.
  int next_handle_;

  DISALLOW_COPY_AND_ASSIGN(CancelableRequestProvider);
};

// CancelableRequestConsumer --------------------------------------------------
//
// Classes wishing to make requests on a provider should have an instance of
// this class. Callers will need to pass a pointer to this consumer object
// when they make the request. It will automatically track any pending
// requests, and will automatically cancel them on destruction to prevent the
// accidental calling of freed memory.
//
// It is recommended to just have this class as a member variable since there
// is nothing to be gained by inheriting from it other than polluting your
// namespace.
//
// THIS CLASS IS NOT THREADSAFE (unlike the provider). You must make requests
// and get callbacks all from the same thread.

// Base class used to notify of new requests.
class CancelableRequestConsumerBase {
 protected:
  friend class CancelableRequestBase;
  friend class CancelableRequestProvider;

  virtual ~CancelableRequestConsumerBase() {
  }

  // Adds a new request to the list of requests that are being tracked. This
  // is called by the provider when a new request is created.
  virtual void OnRequestAdded(CancelableRequestProvider* provider,
                              CancelableRequestProvider::Handle handle) = 0;

  // Removes the given request from the list of pending requests. Called
  // by the CancelableRequest immediately after the callback has executed for a
  // given request, and by the provider when a request is canceled.
  virtual void OnRequestRemoved(CancelableRequestProvider* provider,
                                CancelableRequestProvider::Handle handle) = 0;

  // Sent to provider before executing a callback.
  virtual void WillExecute(CancelableRequestProvider* provider,
                           CancelableRequestProvider::Handle handle) = 0;

  // Sent after executing a callback.
  virtual void DidExecute(CancelableRequestProvider* provider,
                          CancelableRequestProvider::Handle handle) = 0;
};

// Template for clients to use. It allows them to associate random "client
// data" with a specific request. The default value for this type is 0.
// The type T should be small and easily copyable (like a pointer
// or an integer).
template<class T>
class CancelableRequestConsumerTSimple
    : public CancelableRequestConsumerBase {
 public:
  CancelableRequestConsumerTSimple();

  // Cancel any outstanding requests so that we do not get called back after we
  // are destroyed. As these requests are removed, the providers will call us
  // back on OnRequestRemoved, which will then update the list. To iterate
  // successfully while the list is changing out from under us, we make a copy.
  virtual ~CancelableRequestConsumerTSimple();

  // Associates some random data with a specified request. The request MUST be
  // outstanding, or it will assert. This is intended to be called immediately
  // after a request is issued.
  void SetClientData(CancelableRequestProvider* p,
                     CancelableRequestProvider::Handle h,
                     T client_data);

  // Retrieves previously associated data for a specified request. The request
  // MUST be outstanding, or it will assert. This is intended to be called
  // during processing of a callback to retrieve extra data.
  T GetClientData(CancelableRequestProvider* p,
                  CancelableRequestProvider::Handle h);

  // Returns the data associated with the current request being processed. This
  // is only valid during the time a callback is being processed.
  T GetClientDataForCurrentRequest();

  // Returns true if there are any pending requests.
  bool HasPendingRequests() const;

  // Returns the number of pending requests.
  size_t PendingRequestCount() const;

  // Cancels all requests outstanding.
  void CancelAllRequests();

  // Cancels all requests outstanding matching the client data.
  void CancelAllRequestsForClientData(T client_data);

  // Returns the handle for the first request that has the specified client data
  // (in |handle|). Returns true if there is a request for the specified client
  // data, false otherwise.
  bool GetFirstHandleForClientData(T client_data,
                                   CancelableRequestProvider::Handle* handle);

  // Gets the client data for all pending requests.
  void GetAllClientData(std::vector<T>* data);

 protected:
  struct PendingRequest {
    PendingRequest(CancelableRequestProvider* p,
                   CancelableRequestProvider::Handle h)
        : provider(p), handle(h) {
    }

    PendingRequest() : provider(NULL), handle(0) {}

    // Comparison operator for stl.
    bool operator<(const PendingRequest& other) const {
      if (provider != other.provider)
        return provider < other.provider;
      return handle < other.handle;
    }

    bool is_valid() const { return provider != NULL; }

    CancelableRequestProvider* provider;
    CancelableRequestProvider::Handle handle;
  };
  typedef std::map<PendingRequest, T> PendingRequestList;

  virtual T get_initial_t() const;

  virtual void OnRequestAdded(CancelableRequestProvider* provider,
                              CancelableRequestProvider::Handle handle);

  virtual void OnRequestRemoved(CancelableRequestProvider* provider,
                                CancelableRequestProvider::Handle handle);

  virtual void WillExecute(CancelableRequestProvider* provider,
                           CancelableRequestProvider::Handle handle);

  virtual void DidExecute(CancelableRequestProvider* provider,
                          CancelableRequestProvider::Handle handle);

  // Lists all outstanding requests.
  PendingRequestList pending_requests_;

  // This is valid while processing a request and is used to identify the
  // provider/handle of request.
  PendingRequest current_request_;
};

template<class T>
CancelableRequestConsumerTSimple<T>::CancelableRequestConsumerTSimple() {
}

template<class T>
CancelableRequestConsumerTSimple<T>::~CancelableRequestConsumerTSimple() {
  CancelAllRequests();
}

template<class T>
void CancelableRequestConsumerTSimple<T>::SetClientData(
    CancelableRequestProvider* p,
    CancelableRequestProvider::Handle h,
    T client_data) {
  PendingRequest request(p, h);
  DCHECK(pending_requests_.find(request) != pending_requests_.end());
  pending_requests_[request] = client_data;
}

template<class T>
T CancelableRequestConsumerTSimple<T>::GetClientData(
    CancelableRequestProvider* p,
    CancelableRequestProvider::Handle h) {
  PendingRequest request(p, h);
  DCHECK(pending_requests_.find(request) != pending_requests_.end());
  return pending_requests_[request];
}

template<class T>
T CancelableRequestConsumerTSimple<T>::GetClientDataForCurrentRequest() {
  DCHECK(current_request_.is_valid());
  return GetClientData(current_request_.provider, current_request_.handle);
}

template<class T>
bool CancelableRequestConsumerTSimple<T>::HasPendingRequests() const {
  return !pending_requests_.empty();
}

template<class T>
size_t CancelableRequestConsumerTSimple<T>::PendingRequestCount() const {
  return pending_requests_.size();
}

template<class T>
void CancelableRequestConsumerTSimple<T>::CancelAllRequests() {
  // TODO(atwilson): This code is not thread safe as it is called from the
  // consumer thread (via the destructor) and accesses pending_requests_
  // without acquiring the provider lock (http://crbug.com/85970).
  PendingRequestList copied_requests(pending_requests_);
  for (typename PendingRequestList::iterator i = copied_requests.begin();
       i != copied_requests.end(); ++i) {
    i->first.provider->CancelRequest(i->first.handle);
  }
  copied_requests.clear();

  // That should have cleared all the pending items.
  DCHECK(pending_requests_.empty());
}

template<class T>
void CancelableRequestConsumerTSimple<T>::CancelAllRequestsForClientData(
    T client_data) {
  PendingRequestList copied_requests(pending_requests_);
  for (typename PendingRequestList::const_iterator i = copied_requests.begin();
      i != copied_requests.end(); ++i) {
    if (i->second == client_data)
      i->first.provider->CancelRequest(i->first.handle);
  }
  copied_requests.clear();
}

template<class T>
bool CancelableRequestConsumerTSimple<T>::GetFirstHandleForClientData(
    T client_data,
    CancelableRequestProvider::Handle* handle) {
  for (typename PendingRequestList::const_iterator i =
           pending_requests_.begin(); i != pending_requests_.end(); ++i) {
    if (i->second == client_data) {
      *handle = i->first.handle;
      return true;
    }
  }
  *handle = 0;
  return false;
}

template<class T>
void CancelableRequestConsumerTSimple<T>::GetAllClientData(
    std::vector<T>* data) {
  DCHECK(data);
  for (typename PendingRequestList::iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i)
    data->push_back(i->second);
}

template<class T>
T CancelableRequestConsumerTSimple<T>::get_initial_t() const {
  return T();
}

template<class T>
void CancelableRequestConsumerTSimple<T>::OnRequestAdded(
    CancelableRequestProvider* provider,
    CancelableRequestProvider::Handle handle) {
  // Make sure we're not mixing/matching providers (since the provider is
  // responsible for providing thread safety until http://crbug.com/85970 is
  // fixed, we can't use the same consumer with multiple providers).
#ifndef NDEBUG
  if (!pending_requests_.empty())
    DCHECK(pending_requests_.begin()->first.provider == provider);
#endif
  DCHECK(pending_requests_.find(PendingRequest(provider, handle)) ==
         pending_requests_.end());
  pending_requests_[PendingRequest(provider, handle)] = get_initial_t();
}

template<class T>
void CancelableRequestConsumerTSimple<T>::OnRequestRemoved(
    CancelableRequestProvider* provider,
    CancelableRequestProvider::Handle handle) {
  typename PendingRequestList::iterator i =
      pending_requests_.find(PendingRequest(provider, handle));
  if (i == pending_requests_.end()) {
    NOTREACHED() << "Got a complete notification for a nonexistent request";
    return;
  }

  pending_requests_.erase(i);
}

template<class T>
void CancelableRequestConsumerTSimple<T>::WillExecute(
    CancelableRequestProvider* provider,
    CancelableRequestProvider::Handle handle) {
  current_request_ = PendingRequest(provider, handle);
}

template<class T>
void CancelableRequestConsumerTSimple<T>::DidExecute(
    CancelableRequestProvider* provider,
    CancelableRequestProvider::Handle handle) {
  current_request_ = PendingRequest();
}

// See CancelableRequestConsumerTSimple. The default value for T
// is given in |initial_t|.
template<class T, T initial_t>
class CancelableRequestConsumerT
    : public CancelableRequestConsumerTSimple<T> {
 public:
  CancelableRequestConsumerT();
  virtual ~CancelableRequestConsumerT();

 protected:
  virtual T get_initial_t() const;
};

template<class T, T initial_t>
CancelableRequestConsumerT<T, initial_t>::CancelableRequestConsumerT() {
}

template<class T, T initial_t>
CancelableRequestConsumerT<T, initial_t>::~CancelableRequestConsumerT() {
}

template<class T, T initial_t>
T CancelableRequestConsumerT<T, initial_t>::get_initial_t() const {
  return initial_t;
}

// Some clients may not want to store data. Rather than do some complicated
// thing with virtual functions to allow some consumers to store extra data and
// some not to, we just define a default one that stores some dummy data.
typedef CancelableRequestConsumerT<int, 0> CancelableRequestConsumer;

// MSVC doesn't like complex extern templates and DLLs.
#if !defined(COMPILER_MSVC)
// The vast majority of CancelableRequestConsumers are instantiated on <int>,
// so prevent that template from being expanded in normal code.
extern template class CancelableRequestConsumerTSimple<int>;

// We'll also want to extern-template the most common, typedef-ed
// CancelableRequestConsumerT.
extern template class CancelableRequestConsumerT<int, 0>;
#endif

// CancelableRequest ----------------------------------------------------------
//
// The request object that is used by a CancelableRequestProvider to send
// results to a CancelableRequestConsumer. This request handles the returning
// of results from a thread where the request is being executed to the thread
// and callback where the results are used. IT SHOULD BE PASSED AS A
// scoped_refptr TO KEEP IT ALIVE.
//
// It does not handle input parameters to the request. The caller must either
// transfer those separately or derive from this class to add the desired
// parameters.
//
// When the processing is complete on this message, the caller MUST call
// ForwardResult() with the return arguments that will be passed to the
// callback. If the request has been canceled, Return is optional (it will not
// do anything). If you do not have to return to the caller, the cancelable
// request system should not be used! (just use regular fire-and-forget tasks).
//
// Callback parameters are passed by value. In some cases, the request will
// want to return a large amount of data (for example, an image). One good
// approach is to derive from the CancelableRequest and make the data object
// (for example, a std::vector) owned by the CancelableRequest. The pointer
// to this data would be passed for the callback parameter. Since the
// CancelableRequest outlives the callback call, the data will be valid on the
// other thread for the callback, but will still be destroyed properly.

// Non-templatized base class that provides cancellation
class CancelableRequestBase
    : public base::RefCountedThreadSafe<CancelableRequestBase> {
 public:
  friend class CancelableRequestProvider;

  // Initializes most things to empty, Init() must be called to complete
  // initialization of the object. This will be done by the provider when the
  // request is dispatched.
  //
  // This must be called on the same thread the callback will be executed on, it
  // will save that thread for later.
  //
  // This two-phase init is done so that the constructor can have no parameters,
  // which makes it much more convenient for derived classes, which can be
  // common. The derived classes need only declare the variables they provide in
  // the constructor rather than many lines of internal tracking data that are
  // passed to the base class (us).
  //
  // In addition, not all of the information (for example, the handle) is known
  // at construction time.
  CancelableRequestBase();

  CancelableRequestConsumerBase* consumer() const {
    return consumer_;
  }

  CancelableRequestProvider::Handle handle() const {
    return handle_;
  }

  // The canceled flag indicates that the request should not be executed.
  // A request can never be uncanceled, so only a setter for true is provided.
  // This can be called multiple times, but only from one thread.
  void set_canceled() {
    canceled_.Set();
  }
  bool canceled() {
    return canceled_.IsSet();
  }

 protected:
  friend class base::RefCountedThreadSafe<CancelableRequestBase>;
  virtual ~CancelableRequestBase();

  // Initializes the object with the particulars from the provider. It may only
  // be called once (it is called by the provider, which is a friend).
  void Init(CancelableRequestProvider* provider,
            CancelableRequestProvider::Handle handle,
            CancelableRequestConsumerBase* consumer);

  // Attempts to execute callback on |callback_thread_| if the request has not
  // been canceled yet.
  void DoForward(const base::Closure& forwarded_call, bool force_async);

  // Tells the provider that the request is complete, which then tells the
  // consumer.
  void NotifyCompleted() const {
    provider_->RequestCompleted(handle());
    consumer_->DidExecute(provider_, handle_);
  }

  // Cover method for CancelableRequestConsumerBase::WillExecute.
  void WillExecute() {
    consumer_->WillExecute(provider_, handle_);
  }

  // The message loop that this request was created on. The callback will
  // happen on the same thread.
  base::MessageLoop* callback_thread_;

  // The provider for this request. When we execute, we will notify this that
  // request is complete to it can remove us from the requests it tracks.
  CancelableRequestProvider* provider_;

  // Notified after we execute that the request is complete.  This should only
  // be accessed if !canceled_.IsSet(), otherwise the pointer is invalid.
  CancelableRequestConsumerBase* consumer_;

  // The handle to this request inside the provider. This will be initialized
  // to 0 when the request is created, and the provider will set it once the
  // request has been dispatched.
  CancelableRequestProvider::Handle handle_;

  // Set if the caller cancels this request. No callbacks should be made when
  // this is set.
  base::CancellationFlag canceled_;

 private:
  // Executes the |forwarded_call| and notifies the provider and the consumer
  // that this request has been completed. This must be called on the
  // |callback_thread_|.
  void ExecuteCallback(const base::Closure& forwarded_call);

  DISALLOW_COPY_AND_ASSIGN(CancelableRequestBase);
};

// Templatized class. This is the one you should use directly or inherit from.
// The callback can be invoked by calling the ForwardResult() method. For this,
// you must either pack the parameters into a tuple, or use DispatchToMethod
// (in tuple.h).
//
// If you inherit to add additional input parameters or to do more complex
// memory management (see the bigger comment about this above), you can put
// those on a subclass of this.
//
// We have decided to allow users to treat derived classes of this as structs,
// so you can add members without getters and setters (which just makes the
// code harder to read). Don't use underscores after these vars. For example:
//
//   typedef Callback1<int>::Type DoodieCallback;
//
//   class DoodieRequest : public CancelableRequest<DoodieCallback> {
//    public:
//     DoodieRequest(CallbackType* callback) : CancelableRequest(callback) {
//     }
//
//    private:
//     ~DoodieRequest() {}
//
//     int input_arg1;
//     std::wstring input_arg2;
//   };
template<typename CB>
class CancelableRequest : public CancelableRequestBase {
 public:
  // TODO(ajwong): After all calls have been migrated, remove this template in
  // favor of the specialization on base::Callback<Sig>.
  typedef CB CallbackType;          // CallbackRunner<...>
  typedef typename CB::TupleType TupleType;  // Tuple of the callback args.

  // The provider MUST call Init() (on the base class) before this is valid.
  // This class will take ownership of the callback object and destroy it when
  // appropriate.
  explicit CancelableRequest(CallbackType* callback)
      : CancelableRequestBase(),
        callback_(callback) {
    DCHECK(callback) << "We should always have a callback";
  }

  // Dispatches the parameters to the correct thread so the callback can be
  // executed there. The caller does not need to check for cancel before
  // calling this. It is optional in the cancelled case. In the non-cancelled
  // case, this MUST be called.
  //
  // If there are any pointers in the parameters, they must live at least as
  // long as the request so that it can be forwarded to the other thread.
  // For complex objects, this would typically be done by having a derived
  // request own the data itself.
  void ForwardResult(const TupleType& param) {
    DCHECK(callback_.get());
    if (!canceled()) {
      if (callback_thread_ == base::MessageLoop::current()) {
        // We can do synchronous callbacks when we're on the same thread.
        ExecuteCallback(param);
      } else {
        callback_thread_->PostTask(
            FROM_HERE,
            base::Bind(&CancelableRequest<CB>::ExecuteCallback, this, param));
      }
    }
  }

  // Like |ForwardResult| but this never does a synchronous callback.
  void ForwardResultAsync(const TupleType& param) {
    DCHECK(callback_.get());
    if (!canceled()) {
      callback_thread_->PostTask(
          FROM_HERE,
          base::Bind(&CancelableRequest<CB>::ExecuteCallback, this, param));
    }
  }

 protected:
  virtual ~CancelableRequest() {}

 private:
  // Executes the callback and notifies the provider and the consumer that this
  // request has been completed. This must be called on the callback_thread_.
  void ExecuteCallback(const TupleType& param) {
    if (!canceled_.IsSet()) {
      WillExecute();

      // Execute the callback.
      callback_->RunWithParams(param);
    }

    // Notify the provider that the request is complete. The provider will
    // notify the consumer for us. Note that it is possible for the callback to
    // cancel this request; we must check canceled again.
    if (!canceled_.IsSet())
      NotifyCompleted();
  }

  // This should only be executed if !canceled_.IsSet(),
  // otherwise the pointers may be invalid.
  scoped_ptr<CallbackType> callback_;
};

// CancelableRequest<> wraps a normal base::Callback<> for use in the
// CancelableRequest framework.
//
// When using this class, the provider MUST call Init() (on the base class)
// before this is valid.
//
// The two functions ForwardResult(), and ForwardResultAsync() are used to
// invoke the wrapped callback on the correct thread.  They are the same
// except that ForwardResult() will run the callback synchronously if
// we are already on the right thread.
//
// The caller does not need to check for cancel before calling ForwardResult()
// or ForwardResultAsync(). If the request has not been canceled, one of the
// these MUST be called.
//
// If there are any pointers in the parameters, they must live at least as
// long as the request so that it can be forwarded to the other thread.
// For complex objects, this would typically be done by having a derived
// request own the data itself.
//
// The following specializations handle different arities of base::Callbacks<>.
template<>
class CancelableRequest<base::Closure> : public CancelableRequestBase {
 public:
  typedef base::Closure CallbackType;

  explicit CancelableRequest(const CallbackType& callback)
      : CancelableRequestBase(),
        callback_(callback) {
    DCHECK(!callback.is_null()) << "Callback must be initialized.";
  }

  void ForwardResult(void) {
    if (canceled()) return;
    DoForward(callback_, false);
  }

  void ForwardResultAsync() {
    if (canceled()) return;
    DoForward(callback_, true);
  }

 protected:
  virtual ~CancelableRequest() {}

 private:
  CallbackType callback_;
};

template<typename A1>
class CancelableRequest<base::Callback<void(A1)> >
    : public CancelableRequestBase {
 public:
  typedef base::Callback<void(A1)> CallbackType;

  explicit CancelableRequest(const CallbackType& callback)
      : CancelableRequestBase(),
        callback_(callback) {
    DCHECK(!callback.is_null()) << "Callback must be initialized.";
  }

  void ForwardResult(
      typename base::internal::CallbackParamTraits<A1>::ForwardType a1) {
    if (canceled()) return;
    DoForward(base::Bind(callback_, a1), false);
  }

  void ForwardResultAsync(
      typename base::internal::CallbackParamTraits<A1>::ForwardType a1) {
    if (canceled()) return;
    DoForward(base::Bind(callback_, a1), true);
  }

 protected:
  virtual ~CancelableRequest() {}

 private:
  CallbackType callback_;
};

template<typename A1, typename A2>
class CancelableRequest<base::Callback<void(A1,A2)> >
    : public CancelableRequestBase {
 public:
  typedef base::Callback<void(A1,A2)> CallbackType;

  explicit CancelableRequest(const CallbackType& callback)
      : CancelableRequestBase(),
        callback_(callback) {
    DCHECK(!callback.is_null()) << "Callback must be initialized.";
  }

  void ForwardResult(
      typename base::internal::CallbackParamTraits<A1>::ForwardType a1,
      typename base::internal::CallbackParamTraits<A2>::ForwardType a2) {
    if (canceled()) return;
    DoForward(base::Bind(callback_, a1, a2), false);
  }

  void ForwardResultAsync(
      typename base::internal::CallbackParamTraits<A1>::ForwardType a1,
      typename base::internal::CallbackParamTraits<A2>::ForwardType a2) {
    if (canceled()) return;
    DoForward(base::Bind(callback_, a1, a2), true);
  }

 protected:
  virtual ~CancelableRequest() {}

 private:
  CallbackType callback_;
};

template<typename A1, typename A2, typename A3>
class CancelableRequest<base::Callback<void(A1,A2,A3)> >
    : public CancelableRequestBase {
 public:
  typedef base::Callback<void(A1,A2,A3)> CallbackType;

  explicit CancelableRequest(const CallbackType& callback)
      : CancelableRequestBase(),
        callback_(callback) {
    DCHECK(!callback.is_null()) << "Callback must be initialized.";
  }

  void ForwardResult(
      typename base::internal::CallbackParamTraits<A1>::ForwardType a1,
      typename base::internal::CallbackParamTraits<A2>::ForwardType a2,
      typename base::internal::CallbackParamTraits<A3>::ForwardType a3) {
    if (canceled()) return;
    DoForward(base::Bind(callback_, a1, a2, a3), false);
  }

  void ForwardResultAsync(
      typename base::internal::CallbackParamTraits<A1>::ForwardType a1,
      typename base::internal::CallbackParamTraits<A2>::ForwardType a2,
      typename base::internal::CallbackParamTraits<A3>::ForwardType a3) {
    if (canceled()) return;
    DoForward(base::Bind(callback_, a1, a2, a3), true);
  }

 protected:
  virtual ~CancelableRequest() {}

 private:
  CallbackType callback_;
};

template<typename A1, typename A2, typename A3, typename A4>
class CancelableRequest<base::Callback<void(A1, A2, A3, A4)> >
    : public CancelableRequestBase {
 public:
  typedef base::Callback<void(A1, A2, A3, A4)> CallbackType;

  explicit CancelableRequest(const CallbackType& callback)
      : CancelableRequestBase(),
        callback_(callback) {
    DCHECK(!callback.is_null()) << "Callback must be initialized.";
  }

  void ForwardResult(
      typename base::internal::CallbackParamTraits<A1>::ForwardType a1,
      typename base::internal::CallbackParamTraits<A2>::ForwardType a2,
      typename base::internal::CallbackParamTraits<A3>::ForwardType a3,
      typename base::internal::CallbackParamTraits<A4>::ForwardType a4) {
    if (canceled()) return;
    DoForward(base::Bind(callback_, a1, a2, a3, a4), false);
  }

  void ForwardResultAsync(
      typename base::internal::CallbackParamTraits<A1>::ForwardType a1,
      typename base::internal::CallbackParamTraits<A2>::ForwardType a2,
      typename base::internal::CallbackParamTraits<A3>::ForwardType a3,
      typename base::internal::CallbackParamTraits<A4>::ForwardType a4) {
    if (canceled()) return;
    DoForward(base::Bind(callback_, a1, a2, a3, a4), true);
  }

 protected:
  virtual ~CancelableRequest() {}

 private:
  CallbackType callback_;
};

template<typename A1, typename A2, typename A3, typename A4, typename A5>
class CancelableRequest<base::Callback<void(A1, A2, A3, A4, A5)> >
    : public CancelableRequestBase {
 public:
  typedef base::Callback<void(A1, A2, A3, A4, A5)> CallbackType;

  explicit CancelableRequest(const CallbackType& callback)
      : CancelableRequestBase(),
        callback_(callback) {
    DCHECK(!callback.is_null()) << "Callback must be initialized.";
  }

  void ForwardResult(
      typename base::internal::CallbackParamTraits<A1>::ForwardType a1,
      typename base::internal::CallbackParamTraits<A2>::ForwardType a2,
      typename base::internal::CallbackParamTraits<A3>::ForwardType a3,
      typename base::internal::CallbackParamTraits<A4>::ForwardType a4,
      typename base::internal::CallbackParamTraits<A5>::ForwardType a5) {
    if (canceled()) return;
    DoForward(base::Bind(callback_, a1, a2, a3, a4, a5), false);
  }

  void ForwardResultAsync(
      typename base::internal::CallbackParamTraits<A1>::ForwardType a1,
      typename base::internal::CallbackParamTraits<A2>::ForwardType a2,
      typename base::internal::CallbackParamTraits<A3>::ForwardType a3,
      typename base::internal::CallbackParamTraits<A4>::ForwardType a4,
      typename base::internal::CallbackParamTraits<A5>::ForwardType a5) {
    if (canceled()) return;
    DoForward(base::Bind(callback_, a1, a2, a3, a4, a5), true);
  }

 protected:
  virtual ~CancelableRequest() {}

 private:
  CallbackType callback_;
};

template<typename A1, typename A2, typename A3, typename A4, typename A5,
         typename A6>
class CancelableRequest<base::Callback<void(A1, A2, A3, A4, A5, A6)> >
    : public CancelableRequestBase {
 public:
  typedef base::Callback<void(A1, A2, A3, A4, A5, A6)> CallbackType;

  explicit CancelableRequest(const CallbackType& callback)
      : CancelableRequestBase(),
        callback_(callback) {
    DCHECK(!callback.is_null()) << "Callback must be initialized.";
  }

  void ForwardResult(
      typename base::internal::CallbackParamTraits<A1>::ForwardType a1,
      typename base::internal::CallbackParamTraits<A2>::ForwardType a2,
      typename base::internal::CallbackParamTraits<A3>::ForwardType a3,
      typename base::internal::CallbackParamTraits<A4>::ForwardType a4,
      typename base::internal::CallbackParamTraits<A5>::ForwardType a5,
      typename base::internal::CallbackParamTraits<A6>::ForwardType a6) {
    if (canceled()) return;
    DoForward(base::Bind(callback_, a1, a2, a3, a4, a5, a6), false);
  }

  void ForwardResultAsync(
      typename base::internal::CallbackParamTraits<A1>::ForwardType a1,
      typename base::internal::CallbackParamTraits<A2>::ForwardType a2,
      typename base::internal::CallbackParamTraits<A3>::ForwardType a3,
      typename base::internal::CallbackParamTraits<A4>::ForwardType a4,
      typename base::internal::CallbackParamTraits<A5>::ForwardType a5,
      typename base::internal::CallbackParamTraits<A6>::ForwardType a6) {
    if (canceled()) return;
    DoForward(base::Bind(callback_, a1, a2, a3, a4, a5, a6), true);
  }

 protected:
  virtual ~CancelableRequest() {}

 private:
  CallbackType callback_;
};

// A CancelableRequest with a single value. This is intended for use when
// the provider provides a single value. The provider fills the result into
// the value, and notifies the request with a pointer to the value. For example,
// HistoryService has many methods that callback with a vector. Use the
// following pattern for this:
// 1. Define the callback:
//      typedef Callback2<Handle, std::vector<Foo>*>::Type FooCallback;
// 2. Define the CancelableRequest1 type.
//    typedef CancelableRequest1<FooCallback, std::vector<Foo>> FooRequest;
// 3. The provider method should then fill in the contents of the vector,
//    forwarding the result like so:
//    request->ForwardResult(FooRequest::TupleType(request->handle(),
//                                                 &request->value));
//
// Tip: for passing more than one value, use a Tuple for the value.
template<typename CB, typename Type>
class CancelableRequest1 : public CancelableRequest<CB> {
 public:
  explicit CancelableRequest1(
      const typename CancelableRequest<CB>::CallbackType& callback)
      : CancelableRequest<CB>(callback), value() {
  }

  // The value.
  Type value;

 protected:
  virtual ~CancelableRequest1() {}
};

#endif  // CHROME_BROWSER_COMMON_CANCELABLE_REQUEST_H_
