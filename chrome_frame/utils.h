// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_UTILS_H_
#define CHROME_FRAME_UTILS_H_

#include <atlbase.h>
#include <string>
#include <shdeprecated.h>
#include <urlmon.h>

#include "base/basictypes.h"
#include "base/logging.h"

// utils.h : Various utility functions and classes

extern const wchar_t kChromeContentPrefix[];
extern const wchar_t kChromeProtocolPrefix[];

// This function is very similar to the AtlRegisterTypeLib function except
// that it takes a parameter that specifies whether to register the typelib
// for the current user only or on a machine-wide basis
// Refer to the MSDN documentation for AtlRegisterTypeLib for a description of
// the arguments
HRESULT UtilRegisterTypeLib(HINSTANCE tlb_instance,
                            LPCOLESTR index,
                            bool for_current_user_only);

// This function is very similar to the AtlUnRegisterTypeLib function except
// that it takes a parameter that specifies whether to unregister the typelib
// for the current user only or on a machine-wide basis
// Refer to the MSDN documentation for AtlUnRegisterTypeLib for a description
// of the arguments
HRESULT UtilUnRegisterTypeLib(HINSTANCE tlb_instance,
                              LPCOLESTR index,
                              bool for_current_user_only);

HRESULT UtilRegisterTypeLib(LPCWSTR typelib_path, bool for_current_user_only);

HRESULT UtilUnRegisterTypeLib(LPCWSTR typelib_path, bool for_current_user_only);

HRESULT UtilRegisterTypeLib(ITypeLib* typelib,
                            LPCWSTR typelib_path,
                            LPCWSTR help_dir,
                            bool for_current_user_only);

HRESULT UtilUnRegisterTypeLib(ITypeLib* typelib,
                              bool for_current_user_only);

// Utility function to tell if the NPAPI plugin is registered.
bool UtilIsNPAPIPluginRegistered();

// Sets or clears a marker that causes NPAPI registration to persist across
// updates. The marker is added if set is true and is deleted otherwise.
bool UtilChangePersistentNPAPIMarker(bool set);

// Returns true if the persistent NPAPI marker is set, false otherwise.
bool UtilIsPersistentNPAPIMarkerSet();

// Given an HTML fragment, this function looks for the
// <meta http-equiv="X-UA-Compatible"> tag and extracts the value of the
// "content" attribute
// This method will currently return a false positive if the tag appears
// inside a string in a <SCRIPT> block.
HRESULT UtilGetXUACompatContentValue(const std::wstring& html_string,
                                     std::wstring* content_value);

// Returns a string from ChromeFrame's string table by resource. Must be
// provided with a valid resource id.
std::wstring GetResourceString(int resource_id);

// Displays a message box indicating that there was a version mismatch between
// ChromeFrame and the running instance of Chrome.
// server_version is the version of the running instance of Chrome.
void DisplayVersionMismatchWarning(HWND parent,
                                   const std::string& server_version);

// This class provides a base implementation for ATL modules which want to
// perform all their registration under HKCU. This class overrides the
// RegisterServer and UnregisterServer methods and registers the type libraries
// under HKCU (the rest of the registation is made under HKCU by changing the
// appropriate .RGS files)
template < class BaseAtlModule >
class AtlPerUserModule : public BaseAtlModule {
  public:
  HRESULT RegisterServer(BOOL reg_typelib = FALSE,
                         const CLSID* clsid = NULL) throw() {
    HRESULT hr = BaseAtlModule::RegisterServer(FALSE, clsid);
    if (FAILED(hr)) {
      return hr;
    }
    if (reg_typelib)  {
      hr = UtilRegisterTypeLib(_AtlComModule.m_hInstTypeLib, NULL, false);
    }
    return hr;
  }

  HRESULT UnregisterServer(BOOL unreg_typelib,
                           const CLSID* clsid = NULL) throw() {
    HRESULT hr = BaseAtlModule::UnregisterServer(FALSE, clsid);
    if (FAILED(hr)) {
      return hr;
    }
    if (unreg_typelib)  {
      hr = UtilUnRegisterTypeLib(_AtlComModule.m_hInstTypeLib, NULL, false);
    }
    return hr;
  }
};

// Creates a javascript statement for execution from the function name and
// arguments passed in.
std::string CreateJavascript(const std::string& function_name,
                             const std::string args);

// Use to prevent the DLL from being unloaded while there are still living
// objects with outstanding references.
class AddRefModule {
 public:
  AddRefModule();
  ~AddRefModule();
};

// Retrieves the executable name of the process hosting us. If
// |include_extension| is false, then we strip the extension from the name.
std::wstring GetHostProcessName(bool include_extension);

typedef enum BrowserType {
  BROWSER_INVALID = -1,
  BROWSER_UNKNOWN,
  BROWSER_IE,
  BROWSER_FIREFOX,
  BROWSER_OPERA,
};

BrowserType GetBrowserType();

typedef enum IEVersion {
  IE_INVALID,
  NON_IE,
  IE_UNSUPPORTED,
  IE_6,
  IE_7,
  IE_8,
};

// To get the IE version when Chrome Frame is hosted in IE.  Make sure that
// the hosting browser is IE before calling this function, otherwise NON_IE
// will be returned.
IEVersion GetIEVersion();

// Retrieves the file version from a module handle without extra round trips
// to the disk (as happens with the regular GetFileVersionInfo API).
//
// @param module A handle to the module for which to retrieve the version info.
// @param high On successful return holds the most significant part of the
// file version.  Must be non-null.
// @param low On successful return holds the least significant part of the
// file version.  May be NULL.
// @returns true if the version info was successfully retrieved.
bool GetModuleVersion(HMODULE module, uint32* high, uint32* low);

// Return if the IEXPLORE is in private mode. The IEIsInPrivateBrowsing() checks
// whether current process is IEXPLORE.
bool IsIEInPrivate();

// Calls [ieframe|shdocvw]!DoFileDownload to initiate a download.
HRESULT DoFileDownloadInIE(const wchar_t* url);

// Creates a copy of a menu. We need this when original menu comes from
// a process with higher integrity.
HMENU UtilCloneContextMenu(HMENU original_menu);

// Uses GURL internally to append 'relative' to 'document'
std::string ResolveURL(const std::string& document,
                       const std::string& relative);

// Returns true iff the two urls have the same scheme, same host and same port.
bool HaveSameOrigin(const std::string& url1, const std::string& url2);

// Get a boolean configuration value from registry.
bool GetConfigBool(bool default_value, const wchar_t* value_name);

// Gets an integer configuration value from the registry.
int GetConfigInt(int default_value, const wchar_t* value_name);

// Sets an integer configuration value in the registry.
bool SetConfigInt(const wchar_t* value_name, int value);

// Sets a boolean integer configuration value in the registry.
bool SetConfigBool(const wchar_t* value_name, bool value);

// Deletes the configuration value passed in.
bool DeleteConfigValue(const wchar_t* value_name);

// Returns true if we are running in headless mode in which case we need to
// gather crash dumps, etc to send them to the crash server.
bool IsHeadlessMode();

// Check if this url is opting into Chrome Frame based on static settings.
bool IsOptInUrl(const wchar_t* url);

// A shortcut for QueryService
template <typename T>
HRESULT DoQueryService(const IID& service_id, IUnknown* unk, T** service) {
  DCHECK(service);
  if (!unk)
    return E_INVALIDARG;
  ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = service_provider.QueryFrom(unk);
  if (!service_provider)
    return E_NOINTERFACE;

  hr = service_provider->QueryService(service_id, service);
  if (*service == NULL)
    return E_NOINTERFACE;
  return hr;
}

// Get url (display name) from a moniker, |bind_context| is optional
HRESULT GetUrlFromMoniker(IMoniker* moniker, IBindCtx* bind_context,
                          std::wstring* url);

// Navigates an IWebBrowser2 object to a moniker.
HRESULT NavigateBrowserToMoniker(IUnknown* browser, IMoniker* moniker,
                                 IBindCtx* bind_ctx);

// Raises a flag on the current thread (using TLS) to indicate that an
// in-progress navigation should be rendered in chrome frame.
void MarkBrowserOnThreadForCFNavigation(IBrowserService* browser);

// Checks if this browser instance has been marked as currently navigating
// to a CF document.  If clear_flag is set to true, the tls flag is cleared but
// only if the browser has been marked.
bool CheckForCFNavigation(IBrowserService* browser, bool clear_flag);

// Returns true if the URL passed in is something which can be handled by
// Chrome. If this function returns false then we should fail the navigation.
// When is_privileged is true, chrome extension URLs will be considered valid.
bool IsValidUrlScheme(const std::wstring& url, bool is_privileged);

// Returns the raw http headers for the current request given an
// IWinInetHttpInfo pointer.
std::string GetRawHttpHeaders(IWinInetHttpInfo* info);

// Can be used to determine whether a given request is being performed for
// a sub-frame or iframe in Internet Explorer. This can be called
// from various places, notably in request callbacks and the like.
//
// |service_provider| must not be NULL and should be a pointer to something
// that implements IServiceProvider (if it isn't this method returns false).
//
// Returns true if this method can determine with some certainty that the
// request did NOT originate from a top level frame, returns false otherwise.
bool IsSubFrameRequest(IUnknown* service_provider);

// See COM_INTERFACE_BLIND_DELEGATE below for details.
template <class T>
STDMETHODIMP CheckOutgoingInterface(void* obj, REFIID iid, void** ret,
                                    DWORD cookie) {
  T* instance = reinterpret_cast<T*>(obj);
  HRESULT hr = E_NOINTERFACE;
  IUnknown* delegate = instance ? instance->delegate() : NULL;
  if (delegate) {
    hr = delegate->QueryInterface(iid, ret);
#if !defined(NDEBUG)
    if (SUCCEEDED(hr)) {
      wchar_t iid_string[64] = {0};
      StringFromGUID2(iid, iid_string, arraysize(iid_string));
      DLOG(INFO) << __FUNCTION__ << " Giving out wrapped interface: "
          << iid_string;
    }
#endif
  }

  return hr;
}

// See COM_INTERFACE_ENTRY_IF_DELEGATE_SUPPORTS below for details.
template <class T>
STDMETHODIMP QueryInterfaceIfDelegateSupports(void* obj, REFIID iid,
                                              void** ret, DWORD cookie) {
  HRESULT hr = E_NOINTERFACE;
  T* instance = reinterpret_cast<T*>(obj);
  IUnknown* delegate = instance ? instance->delegate() : NULL;
  if (delegate) {
    ScopedComPtr<IUnknown> original;
    hr = delegate->QueryInterface(iid,
                                  reinterpret_cast<void**>(original.Receive()));
    if (original) {
      IUnknown* supported_interface = reinterpret_cast<IUnknown*>(
          reinterpret_cast<DWORD_PTR>(obj) + cookie);
      supported_interface->AddRef();
      *ret = supported_interface;
      hr = S_OK;
    }
  }

  return hr;
}

// Same as COM_INTERFACE_ENTRY but relies on the class to implement a
// delegate() method that returns a pointer to the delegated COM object.
#define COM_INTERFACE_ENTRY_IF_DELEGATE_SUPPORTS(x) \
    COM_INTERFACE_ENTRY_FUNC(_ATL_IIDOF(x), \
        offsetofclass(x, _ComMapClass), \
        QueryInterfaceIfDelegateSupports<_ComMapClass>)

// Queries the delegated COM object for an interface, bypassing the wrapper.
#define COM_INTERFACE_BLIND_DELEGATE() \
    COM_INTERFACE_ENTRY_FUNC_BLIND(0, CheckOutgoingInterface<_ComMapClass>)

extern const wchar_t kChromeFrameHeadlessMode[];

#endif  // CHROME_FRAME_UTILS_H_
