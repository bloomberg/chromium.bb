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
  // cookie store corresponding to the given window.
  virtual HRESULT GetCookieInfo(const std::string& url,
                                const std::string& name,
                                HWND window, CookieInfo* cookie_info);

  // Constructs a cookie store value given the IE process ID and a set of all
  // IEFrame windows for that process. As defined by the chrome.cookies
  // API, a cookie store value consists of a string ID and a list of all tab
  // IDs belonging to that cookie store. Assigns the API result value with the
  // newly constructed cookie store object.
  // @return True on success. Returns false and also calls
  //     ApiDispatcher::InvocationResult::PostError on failure.
  virtual bool CreateCookieStoreValue(DWORD process_id,
                                      const WindowSet& windows);

  // Finds an IEFrame window belonging to the process associated with the given
  // cookie store ID.
  // If allow_unregistered_store is true, the function succeeds even if the
  // given store ID has not been registered. If it's false, an unregistered
  // store ID will result in a failure and PostError.
  // @return A window associated with the given cookie store, or NULL on error.
  //     Will also call ApiDispatcher::InvocationResult::PostError
  //     on failure to find a window.
  virtual HWND GetWindowFromStoreId(const std::string& store_id,
                                    bool allow_unregistered_store);

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

  // Finds all IEFrame windows and puts them in a map of HWND sets keyed by
  // process ID.
  static void FindAllProcessesAndWindows(ProcessWindowMap* all_windows);

 private:
  // Retrieves all tab IDs from the given window and adds them to the given tab
  // ID list.
  // @return false on error and will also call
  //     ApiDispatcher::InvocationResult::PostError on failures.
  bool AppendToTabIdList(HWND window, ListValue* tab_ids);
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
