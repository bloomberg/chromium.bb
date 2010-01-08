// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_npapi_entrypoints.h"
#include "chrome_frame/chrome_frame_npapi.h"

#define NPAPI WINAPI

// Plugin entry points.
extern "C" {
  NPError NPAPI NP_Initialize(NPNetscapeFuncs* browser_funcs);
  NPError NPAPI NP_GetEntryPoints(NPPluginFuncs* plugin_funcs);
  void NPAPI NP_Shutdown();
}

NPError NPAPI NP_Initialize(NPNetscapeFuncs* browser_funcs) {
  DLOG(INFO) << __FUNCTION__;
  _pAtlModule->Lock();
  npapi::InitializeBrowserFunctions(browser_funcs);
  return NPERR_NO_ERROR;
}

NPError NPAPI NP_GetEntryPoints(NPPluginFuncs* plugin_funcs) {
  plugin_funcs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  plugin_funcs->size = sizeof(plugin_funcs);
  plugin_funcs->newp = chrome_frame::NPP_New;
  plugin_funcs->destroy = chrome_frame::NPP_Destroy;
  plugin_funcs->setwindow = chrome_frame::NPP_SetWindow;
  plugin_funcs->newstream = chrome_frame::NPP_NewStream;
  plugin_funcs->destroystream = chrome_frame::NPP_DestroyStream;
  plugin_funcs->asfile = NULL;
  plugin_funcs->writeready = chrome_frame::NPP_WriteReady;
  plugin_funcs->write = chrome_frame::NPP_Write;
  plugin_funcs->print = chrome_frame::NPP_Print;
  plugin_funcs->event = NULL;
  plugin_funcs->urlnotify = chrome_frame::NPP_URLNotify;
  plugin_funcs->getvalue = chrome_frame::NPP_GetValue;
  plugin_funcs->setvalue = chrome_frame::NPP_SetValue;
  return NPERR_NO_ERROR;
}

void NPAPI NP_Shutdown() {
  DLOG(INFO) << __FUNCTION__;

  npapi::UninitializeBrowserFunctions();

  _pAtlModule->Unlock();  // matches Lock() inside NP_Initialize

  DLOG_IF(ERROR, _pAtlModule->GetLockCount() != 0)
        << "Being shut down but still have " << _pAtlModule->GetLockCount()
        << " living objects";
}


namespace chrome_frame {

NPError NPP_New(NPMIMEType plugin_type, NPP instance, uint16 mode, int16 argc,
                char* argn[], char* argv[], NPSavedData* saved) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  ChromeFrameNPAPI::ChromeFrameNPObject* chrome_frame_npapi_obj =
      reinterpret_cast<ChromeFrameNPAPI::ChromeFrameNPObject*>(
          npapi::CreateObject(instance, ChromeFrameNPAPI::PluginClass()));
  DCHECK(chrome_frame_npapi_obj != NULL);

  ChromeFrameNPAPI* plugin_instance =
      chrome_frame_npapi_obj->chrome_frame_plugin_instance;
  DCHECK(plugin_instance != NULL);

  // Note that we MUST set instance->pdata BEFORE calling Initialize. This is
  // because Initialize can call back into the NPAPI host which will need the
  // pdata field to be set.
  chrome_frame_npapi_obj->chrome_frame_plugin_instance =
      plugin_instance;
  instance->pdata = chrome_frame_npapi_obj;

  bool init = plugin_instance->Initialize(plugin_type, instance,
                                          mode, argc, argn, argv);
  DCHECK(init);

  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  // Takes ownership and releases the object at the end of scope.
  ScopedNpObject<ChromeFrameNPAPI::ChromeFrameNPObject> chrome_frame_npapi_obj(
      reinterpret_cast<ChromeFrameNPAPI::ChromeFrameNPObject*>(
          instance->pdata));

  if (chrome_frame_npapi_obj.get()) {
    ChromeFrameNPAPI* plugin_instance =
        ChromeFrameNPAPI::ChromeFrameInstanceFromPluginInstance(instance);

    plugin_instance->Uninitialize();
    instance->pdata = NULL;
  }

  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window_info) {
  if (window_info == NULL) {
    NOTREACHED();
    return NPERR_GENERIC_ERROR;
  }

  ChromeFrameNPAPI* plugin_instance =
      ChromeFrameNPAPI::ChromeFrameInstanceFromPluginInstance(instance);

  if (plugin_instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  plugin_instance->SetWindow(window_info);
  return NPERR_NO_ERROR;
}

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
                      NPBool seekable, uint16* stream_type) {
  NPAPIUrlRequest* url_request = ChromeFrameNPAPI::ValidateRequest(
      instance, stream->notifyData);
  if (url_request) {
    if (!url_request->OnStreamCreated(type, stream))
      return NPERR_GENERIC_ERROR;
  }

  // We need to return the requested stream mode if we are returning a success
  // code. If we don't do this it causes Opera to blow up.
  *stream_type = NP_NORMAL;
  return NPERR_NO_ERROR;
}

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason) {
  NPAPIUrlRequest* url_request = ChromeFrameNPAPI::ValidateRequest(
      instance, stream->notifyData);
  if (url_request) {
    url_request->OnStreamDestroyed(reason);
  }

  return NPERR_NO_ERROR;
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void* value) {
  if (variable == NPPVpluginScriptableNPObject) {
    void** plugin = reinterpret_cast<void**>(value);
    ChromeFrameNPAPI::ChromeFrameNPObject* chrome_frame_npapi_obj =
        reinterpret_cast<ChromeFrameNPAPI::ChromeFrameNPObject*>(
            instance->pdata);
    // Return value is expected to be retained
    npapi::RetainObject(reinterpret_cast<NPObject*>(chrome_frame_npapi_obj));
    *plugin = chrome_frame_npapi_obj;
    return NPERR_NO_ERROR;
  }
  return NPERR_GENERIC_ERROR;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void* value) {
  return NPERR_GENERIC_ERROR;
}

int32 NPP_WriteReady(NPP instance, NPStream* stream) {
  static const int kMaxBytesForPluginConsumption = 0x7FFFFFFF;

  NPAPIUrlRequest* url_request = ChromeFrameNPAPI::ValidateRequest(
      instance, stream->notifyData);
  if (url_request) {
    return url_request->OnWriteReady();
  }

  return kMaxBytesForPluginConsumption;
}

int32 NPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len,
                void* buffer) {
  NPAPIUrlRequest* url_request = ChromeFrameNPAPI::ValidateRequest(
      instance, stream->notifyData);
  if (url_request) {
    return url_request->OnWrite(buffer, len);
  }

  return len;
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notifyData) {
  ChromeFrameNPAPI* plugin_instance =
      ChromeFrameNPAPI::ChromeFrameInstanceFromPluginInstance(instance);
  if (plugin_instance) {
    plugin_instance->UrlNotify(url, reason, notifyData);
  }
}

void NPP_Print(NPP instance, NPPrint* print_info) {
  ChromeFrameNPAPI* plugin_instance =
      ChromeFrameNPAPI::ChromeFrameInstanceFromPluginInstance(instance);

  if (plugin_instance == NULL) {
    NOTREACHED();
    return;
  }

  plugin_instance->Print(print_info);
}

}  // namespace chrome_frame
