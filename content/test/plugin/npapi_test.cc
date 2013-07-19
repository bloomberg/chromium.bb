// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// npapitest
//
// This is a NPAPI Plugin Program which is used to test the Browser's NPAPI
// host implementation.  It is used in conjunction with the npapi_unittest.
//
// As a NPAPI Plugin, you can invoke it by creating a web page of the following
// type:
//
//     <embed src="content-to-load" type="application/vnd.npapi-test"
//      name="test-name">
//
// arguments:
//     src:      This is the initial content which will be sent to the plugin.
//     type:     Must be "application/vnd.npapi-test"
//     name:     The testcase to run when invoked
//     id:       The id of the test being run (for testing concurrent plugins)
//
// The Plugin drives the actual test, calling host functions and validating the
// Host callbacks which it receives.  It is the duty of the plugin to record
// all errors.
//
// To indicate test completion, the plugin expects the containing HTML page to
// implement two javascript functions:
//     onSuccess(string testname);
//     onFailure(string testname, string results);
// The HTML host pages used in this test will then set a document cookie
// which the automated test framework can poll for and discover that the
// test has completed.
//
//
// TESTS
// When the PluginClient receives a NPP_New callback from the browser,
// it looks at the "name" argument which is passed in.  It verifies that
// the name matches a known test, and instantiates that test.  The test is
// a subclass of PluginTest.
//
//

#include "base/basictypes.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(__GNUC__) && __GNUC__ >= 4
#define EXPORT __attribute__((visibility ("default")))
#else
#define EXPORT
#endif

#include "content/test/plugin/plugin_client.h"

#if defined(OS_WIN)
BOOL API_CALL DllMain(HINSTANCE hDll, DWORD dwReason, LPVOID lpReserved) {
  return TRUE;
}
#endif

extern "C" {
EXPORT NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* pFuncs) {
  return NPAPIClient::PluginClient::GetEntryPoints(pFuncs);
}

EXPORT NPError API_CALL NP_Shutdown() {
  return NPAPIClient::PluginClient::Shutdown();
}

#if defined(OS_WIN) || defined(OS_MACOSX)
EXPORT NPError API_CALL NP_Initialize(NPNetscapeFuncs* npnFuncs) {
  return NPAPIClient::PluginClient::Initialize(npnFuncs);
}
#elif defined(OS_POSIX)
EXPORT NPError API_CALL NP_Initialize(NPNetscapeFuncs* npnFuncs,
                                      NPPluginFuncs* nppFuncs) {
  NPError error = NPAPIClient::PluginClient::Initialize(npnFuncs);
  if (error == NPERR_NO_ERROR) {
    error = NP_GetEntryPoints(nppFuncs);
  }
  return error;
}

EXPORT NPError API_CALL NP_GetValue(NPP instance, NPPVariable variable,
                                    void* value) {
  NPError err = NPERR_NO_ERROR;

  switch (variable) {
    case NPPVpluginNameString:
      *(static_cast<const char**>(value)) = "NPAPI Test Plugin";
      break;
    case NPPVpluginDescriptionString:
      *(static_cast<const char**>(value)) =
          "Simple NPAPI plug-in for Chromium unit tests";
      break;
    case NPPVpluginNeedsXEmbed:
      *(static_cast<NPBool*>(value)) = true;
      break;
    default:
      err = NPERR_GENERIC_ERROR;
      break;
  }

  return err;
}

EXPORT const char* API_CALL NP_GetMIMEDescription(void) {
  // The layout test LayoutTests/fast/js/navigator-mimeTypes-length.html
  // asserts that the number of mimetypes handled by plugins should be
  // greater than the number of plugins.  We specify a mimetype here so
  // this plugin has at least one.
  return "application/vnd.npapi-test:npapitest:test npapi";
}
#endif  // OS_POSIX
} // extern "C"

namespace WebCore {
  const char* currentTextBreakLocaleID() { return "en_us"; }
}
