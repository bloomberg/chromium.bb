// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_UTILS_H_
#define CHROME_FRAME_UTILS_H_

#include <shdeprecated.h>
#include <urlmon.h>
#include <wininet.h>

#include <atlbase.h>
#include <string>

#include "base/basictypes.h"
#include "base/histogram.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/thread.h"
#include "gfx/rect.h"
#include "googleurl/src/gurl.h"

class FilePath;

// utils.h : Various utility functions and classes

extern const wchar_t kChromeContentPrefix[];
extern const char kGCFProtocol[];
extern const wchar_t kChromeProtocolPrefix[];
extern const wchar_t kChromeFrameHeadlessMode[];
extern const wchar_t kChromeFrameAccessibleMode[];
extern const wchar_t kChromeFrameUnpinnedMode[];
extern const wchar_t kAllowUnsafeURLs[];
extern const wchar_t kEnableBuggyBhoIntercept[];
extern const wchar_t kChromeMimeType[];
extern const wchar_t kChromeFrameAttachTabPattern[];
extern const wchar_t kChromeFrameConfigKey[];
extern const wchar_t kRenderInGCFUrlList[];
extern const wchar_t kRenderInHostUrlList[];
extern const wchar_t kEnableGCFRendererByDefault[];
extern const wchar_t kIexploreProfileName[];
extern const wchar_t kRundllProfileName[];

typedef enum ProtocolPatchMethod {
  PATCH_METHOD_IBROWSER = 0,
  PATCH_METHOD_INET_PROTOCOL,  // 1
  PATCH_METHOD_MONIKER,  // 2
};

// A REG_DWORD config value that maps to the ProtocolPatchMethod enum.
// To get the config value, call:
// ProtocolPatchMethod patch_method =
//     static_cast<ProtocolPatchMethod>(
//          GetConfigInt(PATCH_METHOD_IBROWSER, kPatchProtocols));
extern const wchar_t kPatchProtocols[];


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
// under HKCU (the rest of the registration is made under HKCU by changing the
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

FilePath GetIETemporaryFilesFolder();

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

// @returns the module handle to which an address belongs.
HMODULE GetModuleFromAddress(void* address);

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

// Returns true if we are running in accessible mode in which we need to enable
// renderer accessibility for use in automation.
bool IsAccessibleMode();

// Returns true if we are running in unpinned mode in which case DLL
// eviction should be possible.
bool IsUnpinnedMode();

// Returns true if all HTML pages should be rendered in GCF by default.
bool IsGcfDefaultRenderer();

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
  if (service_provider)
    hr = service_provider->QueryService(service_id, service);

  DCHECK(FAILED(hr) || *service);
  return hr;
}

// Navigates an IWebBrowser2 object to a moniker.
// |headers| can be NULL.
HRESULT NavigateBrowserToMoniker(IUnknown* browser, IMoniker* moniker,
                                 const wchar_t* headers, IBindCtx* bind_ctx,
                                 const wchar_t* fragment);

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
bool IsValidUrlScheme(const GURL& url, bool is_privileged);

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

// Thread that enters STA and has a UI message loop.
class STAThread : public base::Thread {
 public:
  explicit STAThread(const char *name) : Thread(name) {}
  bool Start() {
    return StartWithOptions(Options(MessageLoop::TYPE_UI, 0));
  }
 protected:
  // Called just prior to starting the message loop
  virtual void Init() {
    ::CoInitialize(0);
  }

  // Called just after the message loop ends
  virtual void CleanUp() {
    ::CoUninitialize();
  }
};

std::wstring GuidToString(const GUID& guid);

// The urls retrieved from the IMoniker interface don't contain the anchor
// portion of the actual url navigated to. This function checks whether the
// url passed in the bho_url parameter contains an anchor and if yes checks
// whether it matches the url retrieved from the moniker. If yes it returns
// the bho url, if not the moniker url.
std::wstring GetActualUrlFromMoniker(IMoniker* moniker,
                                     IBindCtx* bind_context,
                                     const std::wstring& bho_url);

// Checks if a window is a top level window
bool IsTopLevelWindow(HWND window);

// Seeks a stream back to position 0.
HRESULT RewindStream(IStream* stream);

extern Lock g_ChromeFrameHistogramLock;

// Thread safe versions of the UMA histogram macros we use for ChromeFrame.
// These should be used for histograms in ChromeFrame. If other histogram
// macros from base/histogram.h are needed then thread safe versions of those
// should be defined and used.
#define THREAD_SAFE_UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, \
                                                bucket_count) { \
  AutoLock lock(g_ChromeFrameHistogramLock); \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count); \
}

#define THREAD_SAFE_UMA_HISTOGRAM_TIMES(name, sample) { \
  AutoLock lock(g_ChromeFrameHistogramLock); \
  UMA_HISTOGRAM_TIMES(name, sample); \
}

#define THREAD_SAFE_UMA_HISTOGRAM_COUNTS(name, sample) { \
  AutoLock lock(g_ChromeFrameHistogramLock); \
  UMA_HISTOGRAM_COUNTS(name, sample); \
}

// Fired when we want to notify IE about privacy changes.
#define WM_FIRE_PRIVACY_CHANGE_NOTIFICATION (WM_APP + 1)

// Sent (not posted) when a request needs to be downloaded in the host browser
// instead of Chrome.  WPARAM is 0 and LPARAM is a pointer to an IMoniker
// object.
// NOTE: Since the message is sent synchronously, the handler should only
// start asynchronous operations in order to not block the sender unnecessarily.
#define WM_DOWNLOAD_IN_HOST (WM_APP + 2)

// Maps the InternetCookieState enum to the corresponding CookieAction values
// used for IE privacy stuff.
int32 MapCookieStateToCookieAction(InternetCookieState cookie_state);

// Parses the url passed in and returns a GURL instance without the fragment.
GURL GetUrlWithoutFragment(const wchar_t* url);

// Compares the URLs passed in after removing the fragments from them.
bool CompareUrlsWithoutFragment(const wchar_t* url1, const wchar_t* url2);

// Returns the Referrer from the HTTP headers and additional headers.
std::string FindReferrerFromHeaders(const wchar_t* headers,
                                     const wchar_t* additional_headers);

// Returns the HTTP headers from the binding passed in.
std::string GetHttpHeadersFromBinding(IBinding* binding);

// Returns the HTTP response code from the binding passed in.
int GetHttpResponseStatusFromBinding(IBinding* binding);

// Returns the clipboard format for text/html.
CLIPFORMAT GetTextHtmlClipboardFormat();

// Returns true iff the mime type is text/html.
bool IsTextHtmlMimeType(const wchar_t* mime_type);

// Returns true iff the clipboard format is text/html.
bool IsTextHtmlClipFormat(CLIPFORMAT cf);

// Returns the desired patch method (moniker, http_equiv, protocol sink).
// Defaults to moniker patch.
ProtocolPatchMethod GetPatchMethod();

// Returns true if the IMoniker patch is enabled.
bool IsIBrowserServicePatchEnabled();

// Returns true if we can detect that we are running as SYSTEM, false otherwise.
bool IsSystemProcess();

// STL helper class that implements a functor to delete objects.
// E.g: std::for_each(v.begin(), v.end(), utils::DeleteObject());
namespace utils {
class DeleteObject {
 public:
  template <typename T>
  void operator()(T* obj) {
    delete obj;
  }
};
}

// Convert various protocol flags to text representation. Used for logging.
std::string BindStatus2Str(ULONG bind_status);
std::string PiFlags2Str(DWORD flags);
std::string Bscf2Str(DWORD flags);

// Reads data from a stream into a string.
HRESULT ReadStream(IStream* stream, size_t size, std::string* data);

// Parses urls targeted at ChromeFrame. This class maintains state like
// whether a url is prefixed with the gcf: prefix, whether it is being
// attached to an existing external tab, etc.
class ChromeFrameUrl {
 public:
  ChromeFrameUrl();

  // Parses the url passed in. Returns true on success.
  bool Parse(const std::wstring& url);

  bool is_chrome_protocol() const {
    return is_chrome_protocol_;
  }

  bool attach_to_external_tab() const {
    return attach_to_external_tab_;
  }

  uint64 cookie() const {
    return cookie_;
  }

  int disposition() const {
    return disposition_;
  }

  const gfx::Rect& dimensions() const {
    return dimensions_;
  }

  const GURL& gurl() const {
    return parsed_url_;
  }

  const std::string& profile_name() const {
    return profile_name_;
  }

 private:
  // If we are attaching to an existing external tab, this function parses the
  // suffix portion of the URL which contains the attach_external_tab prefix.
  bool ParseAttachExternalTabUrl();

  // Clear state.
  void Reset();

  bool attach_to_external_tab_;
  bool is_chrome_protocol_;
  uint64 cookie_;
  gfx::Rect dimensions_;
  int disposition_;

  GURL parsed_url_;
  std::string profile_name_;
};

// Returns true if we can navigate to this URL.
// This function checks if the url scheme is valid for navigation within
// chrome and whether it is a restricted URL as per IE settings. In either of
// these cases it returns false.
bool CanNavigate(const GURL& url, IInternetSecurityManager* security_manager,
                 bool is_privileged);

// Utility function that prevents the current module from ever being unloaded.
// Call if you make irreversible patches.
void PinModule();

// Helper function to spin a message loop and dispatch messages while waiting
// for a handle to be signaled.
void WaitWithMessageLoop(HANDLE* handles, int count, DWORD timeout);

#endif  // CHROME_FRAME_UTILS_H_
