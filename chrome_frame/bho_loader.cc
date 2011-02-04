// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/bho_loader.h"

#include <atlbase.h>
#include <atlcomcli.h>
#include <exdisp.h>

#include "chrome_frame/chrome_frame_helper_util.h"
#include "chrome_frame/event_hooker.h"
#include "chrome_tab.h"  // NOLINT


// Describes the window class we look for.
const wchar_t kStatusBarWindowClass[] = L"msctls_statusbar32";

// On IE9, the status bar is disabled by default, so we look for an
// AsyncBoundaryLayer window instead.
const wchar_t kAsyncBoundaryDnWindow[] = L"asynclayerboundarydn\0";

BHOLoader::BHOLoader() : hooker_(new EventHooker()) {
}

BHOLoader::~BHOLoader() {
  if (hooker_) {
    delete hooker_;
    hooker_ = NULL;
  }
}

void BHOLoader::OnHookEvent(DWORD event, HWND window) {
  // Step 1: Make sure that we are in a process named iexplore.exe.
  if (IsNamedProcess(L"iexplore.exe")) {
    if (!IsWindowOfClass(window, kStatusBarWindowClass) &&
        !IsWindowOfClass(window, kAsyncBoundaryDnWindow)) {
      return;
    } else {
      // We have the right sort of window, check to make sure it was created
      // on the current thread.
      DWORD thread_id = GetWindowThreadProcessId(window, NULL);
      _ASSERTE(thread_id == GetCurrentThreadId());
    }

    // Step 2: Check to see if the window is of the right class.
    HWND browser_hwnd = NULL;
    if (IsWindowOfClass(window, kStatusBarWindowClass)) {
      // For IE8 and under, IE loads BHOs in the WM_CREATE handler of the tab
      // window approximately after it creates the status bar window. To be as
      // close to IE as possible in our simulation on BHO loading, we watch for
      // the status bar to be created and do our simulated BHO loading at that
      // time.
      browser_hwnd = GetParent(window);
    } else if (IsWindowOfClass(window, kAsyncBoundaryDnWindow)) {
      // For IE9, the status bar is disabled by default, so we look for an
      // AsyncBoundaryWindow to be created. When we find that, look for a
      // child window owned by the current thread named "tabwindowclass".
      // That will be our browser window.
      browser_hwnd = RecurseFindWindow(NULL, L"tabwindowclass", NULL,
                                       GetCurrentThreadId(),
                                       GetCurrentProcessId());
      _ASSERTE(NULL != browser_hwnd);
    }

    if (browser_hwnd != NULL) {
      // Step 3:
      // Parent window of status bar window is the web browser window. Try to
      // get its IWebBrowser2 interface
      CComPtr<IWebBrowser2> browser;
      UtilGetWebBrowserObjectFromWindow(browser_hwnd, __uuidof(browser),
                                        reinterpret_cast<void**>(&browser));
      if (browser) {
        // Figure out if we're already in the property map.
        wchar_t bho_clsid_as_string[MAX_PATH] = {0};
        StringFromGUID2(CLSID_ChromeFrameBHO, bho_clsid_as_string,
                        ARRAYSIZE(bho_clsid_as_string));
        CComBSTR bho_clsid_as_string_bstr(bho_clsid_as_string);

        CComVariant existing_bho;
        HRESULT hr = browser->GetProperty(bho_clsid_as_string_bstr,
                                          &existing_bho);

        if (existing_bho.vt != VT_DISPATCH || existing_bho.pdispVal == NULL) {
          // Step 4:
          // We have the IWebBrowser2 interface. Now create the BHO instance
          CComPtr<IObjectWithSite> bho_object;
          hr =  bho_object.CoCreateInstance(CLSID_ChromeFrameBHO,
                                            NULL,
                                            CLSCTX_INPROC_SERVER);

          _ASSERTE(bho_object);
          if (SUCCEEDED(hr) && bho_object) {
            // Step 5:
            // Initialize the BHO by calling SetSite and passing it IWebBrowser2
            hr = bho_object->SetSite(browser);
            _ASSERTE(bho_object);
            if (SUCCEEDED(hr)) {
              // Step 6:
              // Now add the BHO to the collection of automation objects. This
              // will ensure that BHO will be accessible from the web pages as
              // any other BHO. Importantly, it will make sure that our BHO
              // will be cleaned up at the right time along with other BHOs.
              CComVariant object_variant(bho_object);
              browser->PutProperty(bho_clsid_as_string_bstr, object_variant);
            }
          }
        }
      }
    }
  }
}

bool BHOLoader::StartHook() {
  return hooker_->StartHook();
}

void BHOLoader::StopHook() {
  hooker_->StopHook();
}

BHOLoader* BHOLoader::GetInstance() {
  static BHOLoader loader;
  return &loader;
}
