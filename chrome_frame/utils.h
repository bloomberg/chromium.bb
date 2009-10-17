// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_UTILS_H_
#define CHROME_FRAME_UTILS_H_

#include <atlbase.h>
#include <string>

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

// Given an HTML fragment, this function looks for the
// <meta http-equiv="X-UA-Compatible"> tag and extracts the value of the
// "content" attribute
// This method will currently return a false positive if the tag appears
// inside a string in a <SCRIPT> block.
HRESULT UtilGetXUACompatContentValue(const std::wstring& html_string,
                                     std::wstring* content_value);


// Appends |suffix| to the substring |channel_name| of |string| iff
// the first instance of |channel_name| in |string| is not already followed by
// |suffix|.
// Returns true if |string| was modified.
bool AppendSuffixToChannelName(std::wstring* string,
                               const std::wstring& channel_name,
                               const std::wstring& suffix);

// Removes |suffix| from |string| if |string| contains |channel_name| followed
// by |suffix|.
// Returns true if |string| was modified.
bool RemoveSuffixFromChannelName(std::wstring* string,
                                 const std::wstring& channel_name,
                                 const std::wstring& suffix);

// Looks for and alters if found the Omaha configuration for Chrome in the
// registry. This changes the auto-update release channel to prevent installed
// builds of Chrome that include Chrome Frame from getting replaced by
// Chrome updates without it.
// Adds the Chrome Frame suffix if add_cf_suffix is true, removes it
// otherwise.
// Returns S_OK if the Chrome Omaha configuration was found and updated.
// Returns S_FALSE if the configuration was found but didn't need updating.
// Returns REGDB_E_READREGDB if the Chrome Omaha key could not be read.
// Returns REGDB_E_WRITEREGDB if the Chrome Omaha key could not be written.
HRESULT UtilUpdateOmahaConfig(bool add_cf_suffix);

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
    return hr;

  return service_provider->QueryService(service_id, service);
}

// Get url (display name) from a moniker, |bind_context| is optional
HRESULT GetUrlFromMoniker(IMoniker* moniker, IBindCtx* bind_context,
                          std::wstring* url);

// Returns true if the URL passed in is something which can be handled by
// Chrome. If this function returns false then we should fail the navigation.
// When is_privileged is true, chrome extension URLs will be considered valid.
bool IsValidUrlScheme(const std::wstring& url, bool is_privileged);

// This returns the base directory in which to store user profiles.
bool GetUserProfileBaseDirectory(std::wstring* path);

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

#endif  // CHROME_FRAME_UTILS_H_
