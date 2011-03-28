// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_H_
#pragma once

#include <string>
#include <list>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"

class ExtensionFunctionDispatcher;
class ListValue;
class Profile;
class QuotaLimitHeuristic;
class Value;

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

// Abstract base class for extension functions the ExtensionFunctionDispatcher
// knows how to dispatch to.
class ExtensionFunction : public base::RefCountedThreadSafe<ExtensionFunction> {
 public:
  ExtensionFunction();

  // Specifies the name of the function.
  void set_name(const std::string& name) { name_ = name; }
  const std::string name() const { return name_; }

  // Set the profile which contains the extension that has originated this
  // function call.
  void set_profile(Profile* profile) { profile_ = profile; }
  Profile* profile() const { return profile_; }

  // Set the id of this function call's extension.
  void set_extension_id(std::string extension_id) {
    extension_id_ = extension_id;
  }
  std::string extension_id() const { return extension_id_; }

  // Specifies the raw arguments to the function, as a JSON value.
  virtual void SetArgs(const ListValue* args) = 0;

  // Retrieves the results of the function as a JSON-encoded string (may
  // be empty).
  virtual const std::string GetResult() = 0;

  // Retrieves any error string from the function.
  virtual const std::string GetError() = 0;

  // Returns a quota limit heuristic suitable for this function.
  // No quota limiting by default.
  virtual void GetQuotaLimitHeuristics(
      std::list<QuotaLimitHeuristic*>* heuristics) const {}

  void set_dispatcher_peer(ExtensionFunctionDispatcher::Peer* peer) {
    peer_ = peer;
  }
  ExtensionFunctionDispatcher* dispatcher() const {
    return peer_->dispatcher_;
  }

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

  // Execute the API. Clients should call set_raw_args() and
  // set_request_id() before calling this method. Derived classes should be
  // ready to return raw_result() and error() before returning from this
  // function.
  virtual void Run() = 0;

 protected:
  friend class base::RefCountedThreadSafe<ExtensionFunction>;

  virtual ~ExtensionFunction();

  // Gets the extension that called this function. This can return NULL for
  // async functions, for example if the extension is unloaded while the
  // function is running.
  const Extension* GetExtension();

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

  // The peer to the dispatcher that will service this extension function call.
  scoped_refptr<ExtensionFunctionDispatcher::Peer> peer_;

  // Id of this request, used to map the response back to the caller.
  int request_id_;

  // The Profile of this function's extension.
  Profile* profile_;

  // The id of this function's extension.
  std::string extension_id_;

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

  DISALLOW_COPY_AND_ASSIGN(ExtensionFunction);
};

// Base class for an extension function that runs asynchronously *relative to
// the browser's UI thread*.
// Note that once Run() returns, dispatcher() can be NULL, so be sure to
// NULL-check.
// TODO(aa) Remove this extra level of inheritance once the browser stops
// parsing JSON (and instead uses custom serialization of Value objects).
class AsyncExtensionFunction : public ExtensionFunction {
 public:
  AsyncExtensionFunction();

  virtual void SetArgs(const ListValue* args);
  virtual const std::string GetResult();
  virtual const std::string GetError();
  virtual void Run();

  // Derived classes should implement this method to do their work and return
  // success/failure.
  virtual bool RunImpl() = 0;

 protected:
  virtual ~AsyncExtensionFunction();

  void SendResponse(bool success);

  // Return true if the argument to this function at |index| was provided and
  // is non-null.
  bool HasOptionalArgument(size_t index);

  // The arguments to the API. Only non-null if argument were specified.
  scoped_ptr<ListValue> args_;

  // The result of the API. This should be populated by the derived class before
  // SendResponse() is called.
  scoped_ptr<Value> result_;

  // Any detailed error from the API. This should be populated by the derived
  // class before Run() returns.
  std::string error_;

  // Any class that gets a malformed message should set this to true before
  // returning.  The calling renderer process will be killed.
  bool bad_message_;

  DISALLOW_COPY_AND_ASSIGN(AsyncExtensionFunction);
};

// A SyncExtensionFunction is an ExtensionFunction that runs synchronously
// *relative to the browser's UI thread*. Note that this has nothing to do with
// running synchronously relative to the extension process. From the extension
// process's point of view, the function is still asynchronous.
//
// This kind of function is convenient for implementing simple APIs that just
// need to interact with things on the browser UI thread.
class SyncExtensionFunction : public AsyncExtensionFunction {
 public:
  SyncExtensionFunction();

  // Derived classes should implement this method to do their work and return
  // success/failure.
  virtual bool RunImpl() = 0;

  virtual void Run();

 protected:
  virtual ~SyncExtensionFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncExtensionFunction);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_H_
