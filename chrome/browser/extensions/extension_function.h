// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_H_
#pragma once

#include <list>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_view_host_observer.h"
#include "ipc/ipc_message.h"

class Browser;
class ChromeRenderMessageFilter;
class ExtensionFunction;
class ExtensionFunctionDispatcher;
class UIThreadExtensionFunction;
class IOThreadExtensionFunction;
class Profile;
class QuotaLimitHeuristic;
class RenderViewHost;

namespace base {
class ListValue;
class Value;
}

#define EXTENSION_FUNCTION_VALIDATE(test) do { \
    if (!(test)) { \
      bad_message_ = true; \
      return false; \
    } \
  } while (0)

#define EXTENSION_FUNCTION_ERROR(error) do { \
    error_ = error; \
    bad_message_ = true; \
    return false; \
  } while (0)

#define DECLARE_EXTENSION_FUNCTION_NAME(name) \
  public: static const char* function_name() { return name; }

// Traits that describe how ExtensionFunction should be deleted. This just calls
// the virtual "Destruct" method on ExtensionFunction, allowing derived classes
// to override the behavior.
struct ExtensionFunctionDeleteTraits {
 public:
  static void Destruct(const ExtensionFunction* x);
};

// Abstract base class for extension functions the ExtensionFunctionDispatcher
// knows how to dispatch to.
class ExtensionFunction
    : public base::RefCountedThreadSafe<ExtensionFunction,
                                        ExtensionFunctionDeleteTraits> {
 public:
  ExtensionFunction();

  virtual UIThreadExtensionFunction* AsUIThreadExtensionFunction();
  virtual IOThreadExtensionFunction* AsIOThreadExtensionFunction();

  // Execute the API. Clients should initialize the ExtensionFunction using
  // SetArgs(), set_request_id(), and the other setters before calling this
  // method. Derived classes should be ready to return GetResult() and
  // GetError() before returning from this function.
  // Note that once Run() returns, dispatcher() can be NULL, so be sure to
  // NULL-check.
  virtual void Run();

  // Returns a quota limit heuristic suitable for this function.
  // No quota limiting by default.
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const {}

  // Called when the quota limit has been exceeded. The default implementation
  // returns an error.
  virtual void OnQuotaExceeded();

  // Specifies the raw arguments to the function, as a JSON value.
  virtual void SetArgs(const base::ListValue* args);

  // Retrieves the results of the function as a JSON-encoded string (may
  // be empty).
  virtual const std::string GetResult();

  // Retrieves the results of the function as a Value.
  base::Value* GetResultValue();

  // Retrieves any error string from the function.
  virtual const std::string GetError();

  // Specifies the name of the function.
  void set_name(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  void set_profile_id(void* profile_id) { profile_id_ = profile_id; }
  void* profile_id() const { return profile_id_; }

  void set_extension(const Extension* extension) { extension_ = extension; }
  const Extension* GetExtension() const { return extension_.get(); }
  const std::string& extension_id() const { return extension_->id(); }

  void set_request_id(int request_id) { request_id_ = request_id; }
  int request_id() { return request_id_; }

  void set_source_url(const GURL& source_url) { source_url_ = source_url; }
  const GURL& source_url() { return source_url_; }

  void set_has_callback(bool has_callback) { has_callback_ = has_callback; }
  bool has_callback() { return has_callback_; }

  void set_include_incognito(bool include) { include_incognito_ = include; }
  bool include_incognito() { return include_incognito_; }

  void set_user_gesture(bool user_gesture) { user_gesture_ = user_gesture; }
  bool user_gesture() const { return user_gesture_; }

 protected:
  friend struct ExtensionFunctionDeleteTraits;

  virtual ~ExtensionFunction();

  // Helper method for ExtensionFunctionDeleteTraits. Deletes this object.
  virtual void Destruct() const = 0;

  // Derived classes should implement this method to do their work and return
  // success/failure.
  virtual bool RunImpl() = 0;

  // Sends the result back to the extension.
  virtual void SendResponse(bool success) = 0;

  // Common implementation for SendResponse.
  void SendResponseImpl(base::ProcessHandle process,
                        IPC::Message::Sender* ipc_sender,
                        int routing_id,
                        bool success);

  // Called when we receive an extension api request that is invalid in a way
  // that JSON validation in the renderer should have caught. This should never
  // happen and could be an attacker trying to exploit the browser, so we crash
  // the renderer instead.
  void HandleBadMessage(base::ProcessHandle process);

  // Return true if the argument to this function at |index| was provided and
  // is non-null.
  bool HasOptionalArgument(size_t index);

  // Id of this request, used to map the response back to the caller.
  int request_id_;

  // The Profile of this function's extension.
  void* profile_id_;

  // The extension that called this function.
  scoped_refptr<const Extension> extension_;

  // The name of this function.
  std::string name_;

  // The URL of the frame which is making this request
  GURL source_url_;

  // True if the js caller provides a callback function to receive the response
  // of this call.
  bool has_callback_;

  // True if this callback should include information from incognito contexts
  // even if our profile_ is non-incognito. Note that in the case of a "split"
  // mode extension, this will always be false, and we will limit access to
  // data from within the same profile_ (either incognito or not).
  bool include_incognito_;

  // True if the call was made in response of user gesture.
  bool user_gesture_;

  // The arguments to the API. Only non-null if argument were specified.
  scoped_ptr<base::ListValue> args_;

  // The result of the API. This should be populated by the derived class before
  // SendResponse() is called.
  scoped_ptr<base::Value> result_;

  // Any detailed error from the API. This should be populated by the derived
  // class before Run() returns.
  std::string error_;

  // Any class that gets a malformed message should set this to true before
  // returning.  The calling renderer process will be killed.
  bool bad_message_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionFunction);
};

// Extension functions that run on the UI thread. Most functions fall into
// this category.
class UIThreadExtensionFunction : public ExtensionFunction {
 public:
  // A delegate for use in testing, to intercept the call to SendResponse.
  class DelegateForTests {
   public:
    virtual void OnSendResponse(UIThreadExtensionFunction* function,
                                bool success) = 0;
  };

  UIThreadExtensionFunction();

  virtual UIThreadExtensionFunction* AsUIThreadExtensionFunction() OVERRIDE;

  void set_test_delegate(DelegateForTests* delegate) {
    delegate_ = delegate;
  }

  // Called when a message was received.
  // Should return true if it processed the message.
  virtual bool OnMessageReceivedFromRenderView(const IPC::Message& message);

  // Set the profile which contains the extension that has originated this
  // function call.
  void set_profile(Profile* profile) { profile_ = profile; }
  Profile* profile() const { return profile_; }

  void SetRenderViewHost(RenderViewHost* render_view_host);
  RenderViewHost* render_view_host() const { return render_view_host_; }

  void set_dispatcher(
      const base::WeakPtr<ExtensionFunctionDispatcher>& dispatcher) {
    dispatcher_ = dispatcher;
  }
  ExtensionFunctionDispatcher* dispatcher() const {
    return dispatcher_.get();
  }

 protected:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class DeleteTask<UIThreadExtensionFunction>;

  virtual ~UIThreadExtensionFunction();

  virtual void SendResponse(bool success) OVERRIDE;

  // Gets the "current" browser, if any.
  //
  // Many extension APIs operate relative to the current browser, which is the
  // browser the calling code is running inside of. For example, popups, tabs,
  // and infobars all have a containing browser, but background pages and
  // notification bubbles do not.
  //
  // If there is no containing window, the current browser defaults to the
  // foremost one.
  //
  // Incognito browsers are not considered unless the calling extension has
  // incognito access enabled.
  //
  // This method can return NULL if there is no matching browser, which can
  // happen if only incognito windows are open, or early in startup or shutdown
  // shutdown when there are no active windows.
  Browser* GetCurrentBrowser();

  // The dispatcher that will service this extension function call.
  base::WeakPtr<ExtensionFunctionDispatcher> dispatcher_;

  // The RenderViewHost we will send responses too.
  RenderViewHost* render_view_host_;

  // The Profile of this function's extension.
  Profile* profile_;

 private:
  // Helper class to track the lifetime of ExtensionFunction's RenderViewHost
  // pointer and NULL it out when it dies. It also allows us to filter IPC
  // messages comming from the RenderViewHost. We use this separate class
  // (instead of implementing NotificationObserver on ExtensionFunction) because
  // it is/ common for subclasses of ExtensionFunction to be
  // NotificationObservers, and it would be an easy error to forget to call the
  // base class's Observe() method.
  class RenderViewHostTracker : public content::NotificationObserver,
                                public content::RenderViewHostObserver {
   public:
    RenderViewHostTracker(UIThreadExtensionFunction* function,
                          RenderViewHost* render_view_host);
   private:
    virtual void Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) OVERRIDE;

    virtual void RenderViewHostDestroyed(
        RenderViewHost* render_view_host) OVERRIDE;
    virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

    UIThreadExtensionFunction* function_;
    content::NotificationRegistrar registrar_;

    DISALLOW_COPY_AND_ASSIGN(RenderViewHostTracker);
  };

  virtual void Destruct() const OVERRIDE;

  scoped_ptr<RenderViewHostTracker> tracker_;

  DelegateForTests* delegate_;
};

// Extension functions that run on the IO thread. This type of function avoids
// a roundtrip to and from the UI thread (because communication with the
// extension process happens on the IO thread). It's intended to be used when
// performance is critical (e.g. the webRequest API which can block network
// requests). Generally, UIThreadExtensionFunction is more appropriate and will
// be easier to use and interface with the rest of the browser.
class IOThreadExtensionFunction : public ExtensionFunction {
 public:
  IOThreadExtensionFunction();

  virtual IOThreadExtensionFunction* AsIOThreadExtensionFunction() OVERRIDE;

  void set_ipc_sender(base::WeakPtr<ChromeRenderMessageFilter> ipc_sender,
                      int routing_id) {
    ipc_sender_ = ipc_sender;
    routing_id_ = routing_id;
  }
  ChromeRenderMessageFilter* ipc_sender() const { return ipc_sender_.get(); }
  int routing_id() const { return routing_id_; }

  base::WeakPtr<ChromeRenderMessageFilter> ipc_sender_weak() const {
    return ipc_sender_;
  }

  void set_extension_info_map(const ExtensionInfoMap* extension_info_map) {
    extension_info_map_ = extension_info_map;
  }
  const ExtensionInfoMap* extension_info_map() const {
    return extension_info_map_.get();
  }

 protected:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class DeleteTask<IOThreadExtensionFunction>;

  virtual ~IOThreadExtensionFunction();

  virtual void Destruct() const OVERRIDE;

  virtual void SendResponse(bool success) OVERRIDE;

 private:
  base::WeakPtr<ChromeRenderMessageFilter> ipc_sender_;
  int routing_id_;

  scoped_refptr<const ExtensionInfoMap> extension_info_map_;
};

// Base class for an extension function that runs asynchronously *relative to
// the browser's UI thread*.
class AsyncExtensionFunction : public UIThreadExtensionFunction {
 public:
  AsyncExtensionFunction();

 protected:
  virtual ~AsyncExtensionFunction();
};

// A SyncExtensionFunction is an ExtensionFunction that runs synchronously
// *relative to the browser's UI thread*. Note that this has nothing to do with
// running synchronously relative to the extension process. From the extension
// process's point of view, the function is still asynchronous.
//
// This kind of function is convenient for implementing simple APIs that just
// need to interact with things on the browser UI thread.
class SyncExtensionFunction : public UIThreadExtensionFunction {
 public:
  SyncExtensionFunction();

  virtual void Run() OVERRIDE;

 protected:
  virtual ~SyncExtensionFunction();
};

class SyncIOThreadExtensionFunction : public IOThreadExtensionFunction {
 public:
  SyncIOThreadExtensionFunction();

  virtual void Run() OVERRIDE;

 protected:
  virtual ~SyncIOThreadExtensionFunction();
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_H_
