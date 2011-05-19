// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/default_plugin/plugin_main.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/default_plugin/plugin_impl.h"
#include "webkit/glue/webkit_glue.h"

namespace default_plugin {

#if defined(OS_WIN)
//
// Forward declare the linker-provided pseudo variable for the
// current module handle.
//
extern "C" IMAGE_DOS_HEADER __ImageBase;

// get the handle to the currently executing module
inline HMODULE GetCurrentModuleHandle() {
  return reinterpret_cast<HINSTANCE>(&__ImageBase);
}
#endif

// Initialized in NP_Initialize.
NPNetscapeFuncs* g_browser = NULL;

NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* funcs) {
  // Be explicit about the namespace, because all internal plugins have
  // functions with these names and some might accidentally put them into the
  // global namespace. In that case, the linker might prefer the global one.
  funcs->version = 11;
  funcs->size = sizeof(*funcs);
  funcs->newp = default_plugin::NPP_New;
  funcs->destroy = default_plugin::NPP_Destroy;
  funcs->setwindow = default_plugin::NPP_SetWindow;
  funcs->newstream = default_plugin::NPP_NewStream;
  funcs->destroystream = default_plugin::NPP_DestroyStream;
  funcs->writeready = default_plugin::NPP_WriteReady;
  funcs->write = default_plugin::NPP_Write;
  funcs->asfile = NULL;
  funcs->print = NULL;
  funcs->event = default_plugin::NPP_HandleEvent;
  funcs->urlnotify = default_plugin::NPP_URLNotify;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  funcs->getvalue = default_plugin::NPP_GetValue;
#else
  funcs->getvalue = NULL;
#endif
  funcs->setvalue = NULL;
  return NPERR_NO_ERROR;
}

NPError API_CALL NP_Initialize(NPNetscapeFuncs* funcs) {
  g_browser = funcs;
  return 0;
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
NPError API_CALL NP_Initialize(NPNetscapeFuncs* funcs, NPPluginFuncs* p_funcs) {
  NPError err = NP_Initialize(funcs);
  if (err != NPERR_NO_ERROR)
    return err;
  return NP_GetEntryPoints(p_funcs);
}
#endif

NPError API_CALL NP_Shutdown(void) {
  g_browser = NULL;
  return NPERR_NO_ERROR;
}

namespace {
// This function is only invoked when the default plugin is invoked
// with a special mime type application/chromium-test-default-plugin
void SignalTestResult(NPP instance) {
  NPObject *window_obj = NULL;
  g_browser->getvalue(instance, NPNVWindowNPObject, &window_obj);
  if (!window_obj) {
    NOTREACHED();
    return;
  }

  std::string script = "javascript:onSuccess()";
  NPString script_string;
  script_string.UTF8Characters = script.c_str();
  script_string.UTF8Length =
      static_cast<unsigned int>(script.length());

  NPVariant result_var;
  g_browser->evaluate(instance, window_obj,
                      &script_string, &result_var);
  g_browser->releaseobject(window_obj);
}

}  // namespace CHROMIUM_DefaultPluginTest

bool NegotiateModels(NPP instance) {
#if defined(OS_MACOSX)
  NPError err;
  // Set drawing model to core graphics
  NPBool supportsCoreGraphics = false;
  err = g_browser->getvalue(instance,
                            NPNVsupportsCoreGraphicsBool,
                            &supportsCoreGraphics);
  if (err != NPERR_NO_ERROR || !supportsCoreGraphics) {
    NOTREACHED();
    return false;
  }
  err = g_browser->setvalue(instance,
                            NPPVpluginDrawingModel,
                            (void*)NPDrawingModelCoreGraphics);
  if (err != NPERR_NO_ERROR) {
    NOTREACHED();
    return false;
  }

  // Set event model to cocoa
  NPBool supportsCocoaEvents = false;
  err = g_browser->getvalue(instance,
                            NPNVsupportsCocoaBool,
                            &supportsCocoaEvents);
  if (err != NPERR_NO_ERROR || !supportsCocoaEvents) {
    NOTREACHED();
    return false;
  }
  err = g_browser->setvalue(instance,
                            NPPVpluginEventModel,
                            (void*)NPEventModelCocoa);
  if (err != NPERR_NO_ERROR) {
    NOTREACHED();
    return false;
  }
#elif defined(OS_POSIX)
  NPError err;
  // Check that chrome still supports xembed.
  NPBool supportsXEmbed = false;
  err = g_browser->getvalue(instance,
                            NPNVSupportsXEmbedBool,
                            &supportsXEmbed);
  if (err != NPERR_NO_ERROR || !supportsXEmbed) {
    NOTREACHED();
    return false;
  }

  // Check that the toolkit is still gtk2.
  NPNToolkitType toolkit;
  err = g_browser->getvalue(instance,
                            NPNVToolkit,
                            &toolkit);
  if (err != NPERR_NO_ERROR || toolkit != NPNVGtk2) {
    NOTREACHED();
    return false;
  }
#endif
  return true;
}

NPError NPP_New(NPMIMEType plugin_type, NPP instance, uint16_t mode,
                int16_t argc, char* argn[], char* argv[], NPSavedData* saved) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if (!NegotiateModels(instance))
    return NPERR_INCOMPATIBLE_VERSION_ERROR;

  PluginInstallerImpl* plugin_impl = new PluginInstallerImpl(mode);
  plugin_impl->Initialize(
#if defined(OS_WIN)
                          GetCurrentModuleHandle(),
#else
                          NULL,
#endif
                          instance, plugin_type, argc,
                          argn, argv);

  instance->pdata = reinterpret_cast<void*>(plugin_impl);

  if (!base::strcasecmp(plugin_type,
      "application/chromium-test-default-plugin")) {
    SignalTestResult(instance);
  }

  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  PluginInstallerImpl* plugin_impl =
    reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  if (plugin_impl) {
    plugin_impl->Shutdown();
    delete plugin_impl;
  }

  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window_info) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if (window_info == NULL) {
    NOTREACHED();
    return NPERR_GENERIC_ERROR;
  }

  PluginInstallerImpl* plugin_impl =
      reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  if (plugin_impl == NULL) {
    NOTREACHED();
    return NPERR_GENERIC_ERROR;
  }

  if (!plugin_impl->NPP_SetWindow(window_info)) {
    delete plugin_impl;
    return NPERR_GENERIC_ERROR;
  }

  return NPERR_NO_ERROR;
}

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
                      NPBool seekable, uint16_t* stype) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  PluginInstallerImpl* plugin_impl =
    reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  if (!plugin_impl) {
    NOTREACHED();
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  plugin_impl->NewStream(stream);
  return NPERR_NO_ERROR;
}

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  PluginInstallerImpl* plugin_impl =
      reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  if (!plugin_impl) {
    NOTREACHED();
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  plugin_impl->DestroyStream(stream, reason);
  return NPERR_NO_ERROR;
}

int32_t NPP_WriteReady(NPP instance, NPStream* stream) {
  if (instance == NULL)
    return 0;

  PluginInstallerImpl* plugin_impl =
      reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  if (!plugin_impl) {
    NOTREACHED();
    return 0;
  }

  if (plugin_impl->WriteReady(stream))
    return 0x7FFFFFFF;
  return 0;
}

int32_t NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len,
                void* buffer) {
  if (instance == NULL)
    return 0;

  PluginInstallerImpl* plugin_impl =
      reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  if (!plugin_impl) {
    NOTREACHED();
    return 0;
  }

  return plugin_impl->Write(stream, offset, len, buffer);
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notifyData) {
  if (instance != NULL) {
    PluginInstallerImpl* plugin_impl =
        reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

    if (!plugin_impl) {
      NOTREACHED();
      return;
    }

    plugin_impl->URLNotify(url, reason);
  }
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
NPError NPP_GetValue(NPP instance, NPPVariable variable, void* value) {
  switch (variable) {
    case NPPVpluginNeedsXEmbed:
      *static_cast<NPBool*>(value) = true;
      return NPERR_NO_ERROR;
    default:
      return NPERR_INVALID_PARAM;
  }
}
#endif

int16_t NPP_HandleEvent(NPP instance, void* event) {
  if (instance == NULL)
    return 0;

  PluginInstallerImpl* plugin_impl =
      reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  return plugin_impl->NPP_HandleEvent(event);
}

}   // default_plugin
