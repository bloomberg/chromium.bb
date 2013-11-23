// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/plugin/plugin_client.h"

#include "base/strings/string_util.h"
#include "content/test/plugin/plugin_execute_stream_javascript.h"
#include "content/test/plugin/plugin_test.h"
#include "content/test/plugin/plugin_test_factory.h"

namespace NPAPIClient {

NPNetscapeFuncs* PluginClient::host_functions_;

NPError PluginClient::GetEntryPoints(NPPluginFuncs* pFuncs) {
  if (pFuncs == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  if (pFuncs->size < sizeof(NPPluginFuncs))
    return NPERR_INVALID_FUNCTABLE_ERROR;

  pFuncs->version       = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  pFuncs->newp          = NPP_New;
  pFuncs->destroy       = NPP_Destroy;
  pFuncs->setwindow     = NPP_SetWindow;
  pFuncs->newstream     = NPP_NewStream;
  pFuncs->destroystream = NPP_DestroyStream;
  pFuncs->asfile        = NPP_StreamAsFile;
  pFuncs->writeready    = NPP_WriteReady;
  pFuncs->write         = NPP_Write;
  pFuncs->print         = NPP_Print;
  pFuncs->event         = NPP_HandleEvent;
  pFuncs->urlnotify     = NPP_URLNotify;
  pFuncs->getvalue      = NPP_GetValue;
  pFuncs->setvalue      = NPP_SetValue;
  pFuncs->javaClass     = NULL;
  pFuncs->urlredirectnotify = NPP_URLRedirectNotify;
  pFuncs->clearsitedata = NPP_ClearSiteData;

  return NPERR_NO_ERROR;
}

NPError PluginClient::Initialize(NPNetscapeFuncs* pFuncs) {
  if (pFuncs == NULL) {
    return NPERR_INVALID_FUNCTABLE_ERROR;
  }

  if (static_cast<unsigned char>((pFuncs->version >> 8) & 0xff) >
      NP_VERSION_MAJOR) {
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }

#if defined(OS_WIN)
  // Check if we should crash.
  HANDLE crash_event = CreateEvent(NULL, TRUE, FALSE, L"TestPluginCrashOnInit");
  if (WaitForSingleObject(crash_event, 0) == WAIT_OBJECT_0) {
    int *zero = NULL;
    *zero = 0;
  }
  CloseHandle(crash_event);
#endif

  host_functions_ = pFuncs;

  return NPERR_NO_ERROR;
}

NPError PluginClient::Shutdown() {
  return NPERR_NO_ERROR;
}

} // namespace NPAPIClient

extern "C" {
NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode,
                int16 argc, char* argn[], char* argv[], NPSavedData* saved) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  NPAPIClient::PluginTest* new_test = NULL;
  if (mode == NP_FULL) {
    new_test = new::NPAPIClient::ExecuteStreamJavaScript(
        instance, NPAPIClient::PluginClient::HostFunctions());
  } else {
    // We look at the test name requested via the plugin arguments.  We match
    // that against a given test and try to instantiate it.

    // lookup the name parameter
    std::string test_name;
    for (int name_index = 0; name_index < argc; name_index++) {
      if (base::strcasecmp(argn[name_index], "name") == 0) {
        test_name = argv[name_index];
        break;
      }
    }
    if (test_name.empty())
      return NPERR_GENERIC_ERROR;  // no name found

    new_test = NPAPIClient::CreatePluginTest(test_name,
        instance, NPAPIClient::PluginClient::HostFunctions());
    if (new_test == NULL) {
      // If we don't have a test case for this, create a
      // generic one which basically never fails.
      LOG(WARNING) << "Unknown test name '" << test_name
                   << "'; using default test.";
      new_test = new NPAPIClient::PluginTest(instance,
          NPAPIClient::PluginClient::HostFunctions());
    }
  }

#if defined(OS_MACOSX)
  // Set modern drawing and event models.
  NPError drawing_ret = NPAPIClient::PluginClient::HostFunctions()->setvalue(
      instance, NPPVpluginDrawingModel, (void*)NPDrawingModelCoreGraphics);
  NPError event_ret = NPAPIClient::PluginClient::HostFunctions()->setvalue(
      instance, NPPVpluginEventModel, (void*)NPEventModelCocoa);
  if (drawing_ret != NPERR_NO_ERROR || event_ret != NPERR_NO_ERROR)
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
#endif

  NPError ret = new_test->New(mode, argc, (const char**)argn,
      (const char**)argv, saved);
  if ((ret == NPERR_NO_ERROR) && new_test->IsWindowless()) {
    NPAPIClient::PluginClient::HostFunctions()->setvalue(
          instance, NPPVpluginWindowBool, NULL);
  }

  return ret;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  NPAPIClient::PluginTest* plugin =
      reinterpret_cast<NPAPIClient::PluginTest*>(instance->pdata);

  NPError rv = plugin->Destroy();
  delete plugin;
  return rv;
}

NPError NPP_SetWindow(NPP instance, NPWindow* pNPWindow) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  NPAPIClient::PluginTest* plugin =
      reinterpret_cast<NPAPIClient::PluginTest*>(instance->pdata);

  return plugin->SetWindow(pNPWindow);
}

NPError NPP_NewStream(NPP instance, NPMIMEType type,
                      NPStream* stream, NPBool seekable, uint16* stype) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  NPAPIClient::PluginTest* plugin =
      reinterpret_cast<NPAPIClient::PluginTest*>(instance->pdata);

  return plugin->NewStream(type, stream, seekable, stype);
}

int32 NPP_WriteReady(NPP instance, NPStream *stream) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  NPAPIClient::PluginTest* plugin =
      reinterpret_cast<NPAPIClient::PluginTest*>(instance->pdata);

  return plugin->WriteReady(stream);
}

int32 NPP_Write(NPP instance, NPStream *stream, int32 offset,
                 int32 len, void *buffer) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  NPAPIClient::PluginTest* plugin =
      reinterpret_cast<NPAPIClient::PluginTest*>(instance->pdata);

  return plugin->Write(stream, offset, len, buffer);
}

NPError NPP_DestroyStream(NPP instance, NPStream *stream, NPError reason) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  NPAPIClient::PluginTest* plugin =
      reinterpret_cast<NPAPIClient::PluginTest*>(instance->pdata);

  return plugin->DestroyStream(stream, reason);
}

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname) {
  if (instance == NULL)
    return;

  NPAPIClient::PluginTest* plugin =
      reinterpret_cast<NPAPIClient::PluginTest*>(instance->pdata);

  return plugin->StreamAsFile(stream, fname);
}

void NPP_Print(NPP instance, NPPrint* printInfo) {
  if (instance == NULL)
    return;

  // XXXMB - do work here.
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notifyData) {
  if (instance == NULL)
    return;

  NPAPIClient::PluginTest* plugin =
      reinterpret_cast<NPAPIClient::PluginTest*>(instance->pdata);

  return plugin->URLNotify(url, reason, notifyData);
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if (variable == NPPVpluginNeedsXEmbed) {
    *static_cast<NPBool*>(value) = 1;
    return NPERR_NO_ERROR;
  }

  // XXXMB - do work here.
  return NPERR_GENERIC_ERROR;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  // XXXMB - do work here.
  return NPERR_GENERIC_ERROR;
}

int16 NPP_HandleEvent(NPP instance, void* event) {
  if (instance == NULL)
    return 0;

  NPAPIClient::PluginTest* plugin =
      reinterpret_cast<NPAPIClient::PluginTest*>(instance->pdata);

  return plugin->HandleEvent(event);
}

void NPP_URLRedirectNotify(NPP instance, const char* url, int32_t status,
                           void* notify_data) {
  if (instance) {
    NPAPIClient::PluginTest* plugin =
        reinterpret_cast<NPAPIClient::PluginTest*>(instance->pdata);
    plugin->URLRedirectNotify(url, status, notify_data);
  }
}

NPError NPP_ClearSiteData(const char* site,
                          uint64 flags,
                          uint64 max_age) {
  VLOG(0) << "NPP_ClearSiteData called";
  return NPERR_NO_ERROR;
}
} // extern "C"
