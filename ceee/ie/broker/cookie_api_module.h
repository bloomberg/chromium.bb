// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Cookie API implementation.

#ifndef CEEE_IE_BROKER_COOKIE_API_MODULE_H_
#define CEEE_IE_BROKER_COOKIE_API_MODULE_H_

#include <list>
#include <map>
#include <set>
#include <string>

#include "ceee/ie/broker/api_dispatcher.h"

#include "toolband.h"  //NOLINT

namespace cookie_api {

class CookieApiResult;
typedef ApiResultCreator<CookieApiResult> CookieApiResultCreator;

extern const char kProtectedModeStoreIdSuffix[];

// Registers all Cookie API Invocations with the given dispatcher.
void RegisterInvocations(ApiDispatcher* dispatcher);

// Helper class to handle the data cleanup.
class CookieInfo : public CeeeCookieInfo {
 public:
  CookieInfo();
  ~CookieInfo();
 private:
  DISALLOW_COPY_AND_ASSIGN(CookieInfo);
};

class CookieApiResult : public ApiDispatcher::InvocationResult {
 public:
  typedef std::set<HWND> WindowSet;
  typedef std::map<DWORD, WindowSet> ProcessWindowMap;

  explicit CookieApiResult(int request_id)
      : ApiDispatcher::InvocationResult(request_id) {
  }

  // Creates a value object with the information for a cookie and assigns the
  // API result value with it.
  // @return True on success. Returns false and also calls
  //     ApiDispatcher::InvocationResult::PostError on failure.
  virtual bool CreateCookieValue(const CookieInfo& info);

  // Gets information for the cookie with the given URL and name, using the
  // cookie store corresponding to the given window and protected mode.
  virtual HRESULT GetCookieInfo(const std::string& url,
                                const std::string& name,
                                HWND window, bool is_protected_mode,
                                CookieInfo* cookie_info);

  // Constructs up to 2 cookie store values given the IE process ID and a set
  // of all IEFrame windows for that process. As defined by the chrome.cookies
  // API, a cookie store value consists of a string ID and a list of all tab
  // IDs belonging to that cookie store. The 2 cookie stores represent the
  // unprotected mode and protected mode cookie stores belonging to the given
  // IE process; each cookie store object will only be created if it contains
  // actual tab windows. The newly constructed cookie store objects are added
  // to the API result value.
  // @return True on success. Returns false and also calls
  //     ApiDispatcher::InvocationResult::PostError on failure.
  virtual bool CreateCookieStoreValues(DWORD process_id,
                                       const WindowSet& windows);

  // Parses the given cookie store ID, finding an arbitrary IEFrame window
  // belonging to the store and identifying whether the cookie store is a
  // Protected Mode store or not.
  // If allow_unregistered_store is true, the function succeeds even if the
  // given store ID has not been registered. If it's false, an unregistered
  // store ID will result in a failure and PostError.
  // The window output parameter will be assigned the window for the store ID,
  // and the is_protected_mode parameter will be set to true if the store is
  // for Protected Mode.
  // @return True on success. Returns false and also calls
  //     ApiDispatcher::InvocationResult::PostError on failure.
  virtual bool GetAnyWindowInStore(const std::string& store_id,
                                   bool allow_unregistered_store,
                                   HWND* window, bool* is_protected_mode);

  // Registers the cookie store for the process corresponding to the given
  // window.
  // @return S_OK on success, failure code on error and will also call
  //     ApiDispatcher::InvocationResult::PostError on failures.
  virtual HRESULT RegisterCookieStore(HWND window);

  // Checks whether the given window's process is a registered cookie store.
  // @return S_OK if the cookie store is registered, S_FALSE if not. Returns
  //     a failure code on error and will also call
  //     ApiDispatcher::InvocationResult::PostError on failures.
  virtual HRESULT CookieStoreIsRegistered(HWND window);

  // Finds a tab window that belongs to the given cookie store window, which
  // matches the given protected mode setting.
  // @return A valid tab window on success, or NULL on failure.
  virtual HWND GetAnyTabInWindow(HWND window, bool is_protected_mode);

  // Gets the protected mode setting for the given tab and returns it via the
  // is_protected_mode output argument.
  // @return false on failure to get the protected mode setting.
  virtual bool GetTabProtectedMode(HWND tab_window, bool* is_protected_mode);

  // Finds all IEFrame windows and puts them in a map of HWND sets keyed by
  // process ID.
  static void FindAllProcessesAndWindows(ProcessWindowMap* all_windows);

 protected:
  // Gets the tab and index list for a window. The result put in the output
  // parameter is a list where the (k/2)th element is the kth tab's handle,
  // and the (k/2+1)th element is the kth tab's index. The caller of this
  // function is responsible for the lifetime of tab_list, if the function
  // returns true.
  // @return true if the tab_list output parameter was properly allocated and
  //     set, false otherwise.
  virtual bool GetTabListForWindow(HWND window,
                                   scoped_ptr<ListValue>* tab_list);

 private:
  // Retrieves all tab IDs from the given window and adds them to the given tab
  // ID lists, adding all unprotected mode tabs to tab_ids and adding all
  // protected mode tabs to protected_tab_ids.
  // @return false on error and will also call
  //     ApiDispatcher::InvocationResult::PostError on failures.
  bool AppendToTabIdLists(HWND window, ListValue* tab_ids,
                          ListValue* protected_tab_ids);
};

typedef IterativeApiResult<CookieApiResult> IterativeCookieApiResult;

class GetCookie : public CookieApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};
class GetAllCookies : public CookieApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};
class SetCookie : public CookieApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};
class RemoveCookie : public CookieApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};
class GetAllCookieStores : public ApiResultCreator<IterativeCookieApiResult> {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};

// Permanent event handler for cookies.onChanged. We define a class for the
// event in order to ease unit testing of the CookieApiResult used by the
// event handler implementation.
class CookieChanged {
 public:
  bool EventHandlerImpl(const std::string& input_args,
                        std::string* converted_args);

  // Static wrapper for the event handler implementation.
  static bool EventHandler(const std::string& input_args,
                           std::string* converted_args,
                           ApiDispatcher* dispatcher);
 protected:
  virtual ~CookieChanged() {}

  // Returns allocated memory; the caller is responsible for
  // freeing it.
  virtual CookieApiResult* CreateApiResult() {
    return new CookieApiResult(CookieApiResult::kNoRequestId);
  }
};

}  // namespace cookie_api

#endif  // CEEE_IE_BROKER_COOKIE_API_MODULE_H_
