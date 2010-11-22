// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Dispatcher and registry for Chrome Extension APIs.

#ifndef CEEE_IE_BROKER_API_DISPATCHER_H_
#define CEEE_IE_BROKER_API_DISPATCHER_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/values.h"

#include "broker_lib.h"  // NOLINT

class ExecutorsManager;

// Keeps a registry of all API Invocation implementations and implements
// the logic needed to deserialize API Invocation requests, dispatch them,
// and serialize and return the response.
class ApiDispatcher {
 public:
  ApiDispatcher() : thread_id_(0) {}
  virtual ~ApiDispatcher() {}

  // Dispatches a Chrome Extensions API request and sends back the response.
  //
  // @param message_text The raw JSON-encoded text of the API request over
  //    the automation interface.
  // @param response Where to return the JSON-encoded response.
  virtual void HandleApiRequest(BSTR message_text, BSTR* response);

  // Fire the given event message to Chrome Frame and potentially use it
  // to complete pending extension API execution.
  //
  // @param event_name The name of the event to fire.
  // @param event_args The JSON encoded event arguments.
  virtual void FireEvent(const char* event_name, const char* event_args);

  // This class holds on the result and can be derived from to generate results
  // that are either specific to tabs or windows for example.
  class InvocationResult {
   public:
    static const int kNoRequestId = -1;
    explicit InvocationResult(int request_id)
        : request_id_(request_id) {
    }
    virtual ~InvocationResult();

    // Post the response string from our current result to Chrome Frame.
    virtual void PostResult();

    // Post the given error string to Chrome Frame.
    virtual void PostError(const std::string& error);

    // Identifies if we are pending a post (for which we need a request id).
    virtual bool Pending() const { return request_id_ != kNoRequestId; }

    // Returns the result of the API invocation as a value object which
    // is still owned by this object - the caller does not take ownership.
    // May return NULL if execution didn't produce any results yet.
    const Value* value() const {
      return value_.get();
    }
    void set_value(Value* value) {
      value_.reset(value);
    }

    // Sets a value in a dictionary. The Value pointer is kept in the
    // dictionary which takes ownership so the caller should NOT deallocate it.
    // Even in case of errors, the value will be deallocated.
    // This value can then be retrieved by GetValue below, this can be useful
    // to keep information in the InvocationResult during async handling.
    virtual void SetValue(const char* name, Value* value);

    // Returns a value set by SetValue above. Note that the value is only
    // valid during the lifetime of the InvocationResult object or until another
    // value is set for the same name. Also, the returned value should NOT
    // be deallocated by the caller, it will get deallocated on destruction
    // of the object, or when another value is set for the same name.
    // Returns NULL if no value of this name have been previously set.
    virtual const Value* GetValue(const char* name);

   protected:
    // A unit test seam.
    virtual ApiDispatcher* GetDispatcher();

    // A helper function to post the given response to Chrome Frame.
    virtual void PostResponseToChromeFrame(const char* response_key,
                                           const std::string& response_str);
    // Where to store temporary values.
    scoped_ptr<DictionaryValue> temp_values_;

    // Where to store the result value.
    scoped_ptr<Value> value_;

    // Invocation request identifier.
    int request_id_;

    DISALLOW_COPY_AND_ASSIGN(InvocationResult);
  };

  // Base class for API Invocation Execution registered with this object.
  // A new execution object is created for each API invocation call even though
  // the state data is stored in the classes deriving from InvocationResult
  // (declared above). This is to allow easier testing of the Invocation
  // implementation by having virtual methods like GetDispatcher to be used
  // as a test seam, or other specific methods (like
  // ApiResultCreator<>::CreateApiResult). Otherwise, Invocation::Execute
  // could be a static callback as the event handlers described below.
  class Invocation {
   public:
    Invocation() {}
    virtual ~Invocation() {}

    // Called when a request to invoke the execution of an API is received.
    //
    // @param args The list value object for the arguments passed.
    // @param request_id The identifier of the request being executed.
    virtual void Execute(const ListValue& args, int request_id) = 0;

   protected:
    // Unit test seam.
    virtual ApiDispatcher* GetDispatcher();

    DISALLOW_COPY_AND_ASSIGN(Invocation);
  };

  // The Invocation factory pointers to be stored in the factory map.
  typedef Invocation* (*InvocationFactory)();

  // The permanent event handlers are stored in a specific map and don't get
  // removed after they are called. They are also registered without user data.
  // The handler can stop the broadcast of the event by returning false.
  typedef bool (*PermanentEventHandler)(const std::string& input_args,
                                        std::string* converted_args,
                                        ApiDispatcher* dispatcher);

  // The ephemeral event handlers are stored in a specific map and they get
  // removed after a successfully completed call or an error (a successful call
  // is one which returns S_OK, S_FALSE is returned to identify that this
  // set of arguments is not the one we were waiting for, so we need to wait
  // some more, and returning a FAILED HRESULT causes the handler to be removed
  // from the queue).
  // EphemeralEventHandlers can specify user data to be passed back to them
  // when the event occurs. The dynamic data allocation and release is the
  // responsibility of the caller/handler pair, the dispatcher only keeps the
  // pointer and passes it back to the handler as is without any post-cleanup.
  // Also note that the Ephemeral event handlers are called after the Permanent
  // handlers which may have had the chance to augment the content of the
  // arguments which are passed to the ephemeral handlers. So these are useful
  // for asynchronous invocation completion.
  // We also pass in the ApiDispatcher for ease of test mocking.
  typedef HRESULT (*EphemeralEventHandler)(const std::string& input_args,
                                           InvocationResult* user_data,
                                           ApiDispatcher* dispatcher);

  // Registers a factory for an API Invocation of a given name.  Note that
  // registering the same name more than once is not permitted.
  //
  // @param function_name The name of the function handled by the Invocation.
  // @param factory The factory function to call to create a new instance of
  //    the Invocation type to handle this function.
  virtual void RegisterInvocation(const char* function_name,
                                  InvocationFactory factory);

  // Registers a permanent handler for an event of a given name.  Note that
  // registering the same name more than once is not permitted.
  //
  // @param event_name The name of the event to handle.
  // @param event_handler The event handler to call to handle the event.
  virtual void RegisterPermanentEventHandler(
      const char* event_name, PermanentEventHandler event_handler);

  // Registers an ephemeral handler for an event of a given name.  Note that
  // registering the same name more than once is permitted. When an event is
  // fired, all ephemeral handlers are called after the permanent one if
  // one was registered.
  //
  // @param event_name The name of the event to handle.
  // @param event_handler The event handler to call to handle the event.
  // @param user_data The data that has to be passed back to the handler.
  virtual void RegisterEphemeralEventHandler(
      const char* event_name,
      EphemeralEventHandler event_handler,
      InvocationResult* user_data);

  // Sets the thread id of the ApiInvocation thread so that we can make sure
  // that we are only ran from that thread. Only used for debug purposes.
  virtual void SetApiInvocationThreadId(DWORD thread_id) {
    thread_id_ = thread_id;
  }

  // Fetch the appropriate executor to execute code for the given window.
  //
  // @param window The window for which we want an executor.
  // @param iid The identifier of the interface we want to work with.
  // @param executor Where to return the requested executor interface pointer.
  virtual void GetExecutor(HWND window, REFIID iid, void** executor);

  // Return a tab handle associated with the id.
  //
  // @param tab_id The tab identifier.
  // @return The corresponding HWND (or INVALID_HANDLE_VALUE if tab_id isn't
  //         found).
  virtual HWND GetTabHandleFromId(int tab_id) const;

  // Return a window handle associated with the id.
  //
  // @param window_id The window identifier.
  // @return The corresponding HWND (or INVALID_HANDLE_VALUE if window_id isn't
  //         found).
  virtual HWND GetWindowHandleFromId(int window_id) const;

  // Return a tab id associated with the HWND.
  //
  // @param tab_handle The tab HWND.
  // @return The corresponding tab id (or 0 if tab_handle isn't found).
  virtual int GetTabIdFromHandle(HWND tab_handle) const;

  // Return a window id associated with the HWND.
  //
  // @param window_handle The window HWND.
  // @return The corresponding window id (or 0 if window_handle isn't found).
  virtual int GetWindowIdFromHandle(HWND window_handle) const;

  // Register the relation between a tab_id and a HWND.
  virtual void SetTabIdForHandle(long tab_id, HWND tab_handle);

  // Unregister the HWND and its corresponding tab_id.
  virtual void DeleteTabHandle(HWND handle);

 protected:
  typedef std::map<std::string, InvocationFactory> FactoryMap;
  FactoryMap factories_;

  typedef std::map<std::string, PermanentEventHandler>
      PermanentEventHandlersMap;
  PermanentEventHandlersMap permanent_event_handlers_;

  // For ephemeral event handlers we also need to keep the user data.
  struct EphemeralEventHandlerTuple {
    EphemeralEventHandlerTuple(
        EphemeralEventHandler handler,
        InvocationResult* user_data)
            : handler(handler), user_data(user_data) {}
    EphemeralEventHandler handler;
    InvocationResult* user_data;
  };

  // We can have a list of ephemeral event handlers for a given event name.
  typedef std::vector<EphemeralEventHandlerTuple>
      EphemeralEventHandlersList;
  typedef std::map<std::string, EphemeralEventHandlersList>
      EphemeralEventHandlersMap;
  EphemeralEventHandlersMap ephemeral_event_handlers_;

  // The thread we are running into.
  DWORD thread_id_;

  // Make sure this is always called from the same thread,
  bool IsRunningInSingleThread();

  DISALLOW_COPY_AND_ASSIGN(ApiDispatcher);
};

// Convenience InvocationFactory implementation.
template <class InvocationType>
ApiDispatcher::Invocation* NewApiInvocation() {
  return new InvocationType();
}

// A singleton that initializes and keeps the ApiDispatcher used by production
// code.
class ProductionApiDispatcher : public ApiDispatcher,
    public Singleton<ProductionApiDispatcher> {
 private:
  // This ensures no construction is possible outside of the class itself.
  friend struct DefaultSingletonTraits<ProductionApiDispatcher>;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ProductionApiDispatcher);
};

// A convenience class that can be derived from by API function classes instead
// of ApiDispatcher::Invocation. Mainly benefits ease of testing, so that we
// can specify a mocked result object.
template<class ResultType = ApiDispatcher::InvocationResult>
class ApiResultCreator : public ApiDispatcher::Invocation {
 protected:
  // Allocates a new instance of ResultType. The caller of this function takes
  // ownership of this instance and is responsible for freeing it.
  virtual ResultType* CreateApiResult(int request_id) {
    return new ResultType(request_id);
  }
};

template<class InvocationBase>
class IterativeApiResult : public InvocationBase {
 public:
  explicit IterativeApiResult(int request_id)
      : InvocationBase(request_id), result_accumulator_(new ListValue()) {
  }

  // Unlike the base class, this hack subverts the process by accumulating
  // responses (positive and errors) rather than posting them right away.
  virtual void PostResult() {
    DCHECK(value() != NULL);
    DCHECK(result_accumulator_ != NULL);

    if (result_accumulator_ != NULL)
      result_accumulator_->Append(value_.release());
  }

  virtual void PostError(const std::string& error) {
    error_accumulator_.push_back(error);
  }

  // Finally post whatever was reported as result.
  virtual void FlushAllPosts() {
    if (AllFailed()) {
      CallRealPostError(error_accumulator_.back());
    } else {
      // Post success as per base class implementation. Declare all is fine
      // even if !AllSucceeded.
      // TODO(motek@google.com): Perhaps we should post a different
      // message post for 'not quite ok, but almost'?
      set_value(result_accumulator_.release());
      CallRealPostResult();
    }
    error_accumulator_.clear();
  }

  bool AllSucceeded() const {
    DCHECK(result_accumulator_ != NULL);
    return result_accumulator_ != NULL && !result_accumulator_->empty() &&
        error_accumulator_.empty();
  }

  bool AllFailed() const {
    DCHECK(result_accumulator_ != NULL);
    return !error_accumulator_.empty() &&
        (result_accumulator_ == NULL || result_accumulator_->empty());
  }

  bool IsEmpty() const {
    DCHECK(result_accumulator_ != NULL);
    return (result_accumulator_ == NULL || result_accumulator_->empty()) &&
        error_accumulator_.empty();
  }

  const std::string LastError() const {
    if (!error_accumulator_.empty())
      return error_accumulator_.back();
    return std::string();
  }

 private:
  // Redirected invocations of base class's 'post function' create test seam.
  virtual void CallRealPostResult() {
    InvocationBase::PostResult();
  }
  virtual void CallRealPostError(const std::string& error) {
    InvocationBase::PostError(error);
  }

  scoped_ptr<ListValue> result_accumulator_;
  std::list<std::string> error_accumulator_;
};


#endif  // CEEE_IE_BROKER_API_DISPATCHER_H_
