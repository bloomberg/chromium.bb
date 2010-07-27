// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_NP_EVENT_LISTENER_H_
#define CHROME_FRAME_NP_EVENT_LISTENER_H_

#include "base/logging.h"

#include "chrome_frame/utils.h"
#include "chrome_frame/np_browser_functions.h"

// Avoid conflicts with basictypes and the gecko sdk.
// (different definitions of uint32).
#define NO_NSPR_10_SUPPORT
#include "third_party/xulrunner-sdk/win/include/dom/nsIDOMEventListener.h"


class nsIDOMElement;

class NpEventDelegate {
 public:
  virtual void OnEvent(const char* event_name) = 0;
};

class NpEventListener {
 public:
  NS_IMETHOD_(nsrefcnt) AddRef() = 0;
  NS_IMETHOD_(nsrefcnt) Release() = 0;
  virtual bool Subscribe(NPP instance,
                         const char* event_names[],
                         int event_name_count) = 0;
  virtual bool Unsubscribe(NPP instance,
                           const char* event_names[],
                           int event_name_count) = 0;

 protected:
  virtual ~NpEventListener() {}
};

// A little helper class to implement simple ref counting
// and assert on single threadedness.
template <class T>
class NpEventListenerBase : public NpEventListener {
 public:
  explicit NpEventListenerBase(NpEventDelegate* delegate)
      : ref_count_(0), delegate_(delegate) {
    DCHECK(delegate_);
    thread_id_ = ::GetCurrentThreadId();
  }

  ~NpEventListenerBase() {
    DCHECK(thread_id_ == ::GetCurrentThreadId());
  }

  NS_IMETHOD_(nsrefcnt) AddRef() {
    DCHECK(thread_id_ == ::GetCurrentThreadId());
    ref_count_++;
    return ref_count_;
  }

  NS_IMETHOD_(nsrefcnt) Release() {
    DCHECK(thread_id_ == ::GetCurrentThreadId());
    ref_count_--;

    if (!ref_count_) {
      T* me = static_cast<T*>(this);
      delete me;
      return 0;
    }

    return ref_count_;
  }

 protected:
  nsrefcnt ref_count_;
  NpEventDelegate* delegate_;
  AddRefModule module_ref_;
  // used to DCHECK on expected single-threaded usage
  unsigned long thread_id_;
};

// Implements nsIDOMEventListener in order to receive events from DOM
// elements inside an HTML page.
class DomEventListener
  : public nsIDOMEventListener,
  public NpEventListenerBase<DomEventListener> {
 public:
  explicit DomEventListener(NpEventDelegate* delegate);
  virtual ~DomEventListener();

  // Implementation of NpEventListener
  virtual bool Subscribe(NPP instance,
                         const char* event_names[],
                         int event_name_count);
  virtual bool Unsubscribe(NPP instance,
                           const char* event_names[],
                           int event_name_count);
 protected:
  // We implement QueryInterface etc ourselves in order to avoid
  // extra dependencies brought on by the NS_IMPL_* macros.
  NS_IMETHOD QueryInterface(REFNSIID iid, void** ptr);
  NS_IMETHOD_(nsrefcnt) AddRef() {
    return NpEventListenerBase<DomEventListener>::AddRef();
  }

  NS_IMETHOD_(nsrefcnt) Release() {
    return NpEventListenerBase<DomEventListener>::Release();
  }

  // Implementation of nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent *event);

 private:
  static bool GetObjectElement(NPP instance, nsIDOMElement** element);

 private:
  DISALLOW_COPY_AND_ASSIGN(DomEventListener);
};

class NPObjectEventListener
  : public NpEventListenerBase<NPObjectEventListener> {
 public:
  explicit NPObjectEventListener(NpEventDelegate* delegate);
  ~NPObjectEventListener();

  // Implementation of NpEventListener
  virtual bool Subscribe(NPP instance,
                         const char* event_names[],
                         int event_name_count);
  virtual bool Unsubscribe(NPP instance,
                           const char* event_names[],
                           int event_name_count);

 protected:
  // NPObject structure which is exposed by NPObjectEventListener.
  class Npo : public NPObject {
   public:
    explicit Npo(NPP npp) : npp_(npp), listener_(NULL) {
    }

    void Initialize(NPObjectEventListener* listener) {
      listener_ = listener;
    }

    inline NPObjectEventListener* listener() const {
      return listener_;
    }

    inline NPP npp() const {
      return npp_;
    }

   protected:
    NPP npp_;
    NPObjectEventListener* listener_;
    AddRefModule module_ref_;
  };

  static NPClass* PluginClass();

  static bool HasMethod(Npo* npo, NPIdentifier name);
  static bool Invoke(Npo* npo, NPIdentifier name, const NPVariant* args,
                     uint32_t arg_count, NPVariant* result);
  static NPObject* AllocateObject(NPP instance, NPClass* class_name);
  static void DeallocateObject(Npo* npo);

  typedef enum {
    HANDLE_EVENT,
    TYPE,
    ADD_EVENT_LISTENER,
    REMOVE_EVENT_LISTENER,
    TAG_NAME,
    PARENT_ELEMENT,
    IDENTIFIER_COUNT,
  } CachedStringIdentifiers;

  static NPIdentifier* GetCachedStringIds();

  void HandleEvent(Npo* npo, NPObject* event);
  static NPObject* GetObjectElement(NPP instance);

 private:
  // Our NPObject.
  ScopedNpObject<Npo> npo_;

  DISALLOW_COPY_AND_ASSIGN(NPObjectEventListener);
};

#endif  // CHROME_FRAME_NP_EVENT_LISTENER_H_
