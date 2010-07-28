// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper_widget.h"

#include "base/hash_tables.h"
#include "base/lazy_instance.h"
#include "chrome/renderer/pepper_scrollbar_widget.h"
#include "chrome/renderer/webplugin_delegate_pepper.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/webplugin.h"
#include "webkit/glue/plugins/webplugin_delegate.h"

#if defined(OS_WIN)
#include "base/win_util.h"
#endif

static int g_next_id;
typedef base::hash_map<int, PepperWidget*> WidgetMap;
static base::LazyInstance<WidgetMap> g_widgets(base::LINKER_INITIALIZED);

NPError NPCreateWidget(NPP instance,
                       NPWidgetType type,
                       void* params,
                       NPWidgetID* id) {
  PepperWidget* widget;
  switch (type) {
    case NPWidgetTypeScrollbar:
      widget = new PepperScrollbarWidget(
          *static_cast<NPScrollbarCreateParams*>(params));
      break;
    default:
      return NPERR_INVALID_PARAM;
  }

  *id = ++g_next_id;
  widget->Init(instance, *id);
  return NPERR_NO_ERROR;
}

NPError NPDestroyWidget(NPP instance, NPWidgetID id) {
  WidgetMap::iterator iter = g_widgets.Get().find(id);
  if (iter == g_widgets.Get().end())
    return NPERR_INVALID_PARAM;

  iter->second->Destroy();
  return NPERR_NO_ERROR;
}

NPError NPPaintWidget(NPP instance,
                      NPWidgetID id,
                      NPDeviceContext2D* context,
                      NPRect* dirty) {
  WidgetMap::iterator iter = g_widgets.Get().find(id);
  if (iter == g_widgets.Get().end())
    return NPERR_INVALID_PARAM;

  NPAPI::PluginInstance* plugin =
      static_cast<NPAPI::PluginInstance*>(instance->ndata);
  WebPluginDelegatePepper* delegate =
      static_cast<WebPluginDelegatePepper*>(plugin->webplugin()->delegate());
  Graphics2DDeviceContext* gdc = delegate->GetGraphicsContext(context);
  iter->second->Paint(gdc, *dirty);

#if defined(OS_WIN)
  if (win_util::GetWinVersion() == win_util::WINVERSION_XP) {
    gdc->canvas()->getTopPlatformDevice().makeOpaque(
        dirty->left, dirty->top, dirty->right - dirty->left,
        dirty->bottom - dirty->top);
  }
#endif
  return NPERR_NO_ERROR;
}

bool NPHandleWidgetEvent(NPP instance, NPWidgetID id, NPPepperEvent* event) {
  WidgetMap::iterator iter = g_widgets.Get().find(id);
  if (iter == g_widgets.Get().end())
    return false;

  return iter->second->HandleEvent(*event);
}

NPError NPGetWidgetProperty(NPP instance,
                            NPWidgetID id,
                            NPWidgetProperty property,
                            void* value) {
  WidgetMap::iterator iter = g_widgets.Get().find(id);
  if (iter == g_widgets.Get().end())
    return NPERR_INVALID_PARAM;

  iter->second->GetProperty(property, value);
  return NPERR_NO_ERROR;
}

NPError NPSetWidgetProperty(NPP instance,
                            NPWidgetID id,
                            NPWidgetProperty property,
                            void* value) {
  WidgetMap::iterator iter = g_widgets.Get().find(id);
  if (iter == g_widgets.Get().end())
    return NPERR_INVALID_PARAM;

  iter->second->SetProperty(property, value);
  return NPERR_NO_ERROR;
}

NPWidgetExtensions g_widget_extensions = {
  NPCreateWidget,
  NPDestroyWidget,
  NPPaintWidget,
  NPHandleWidgetEvent,
  NPGetWidgetProperty,
  NPSetWidgetProperty
};

// static
NPWidgetExtensions* PepperWidget::GetWidgetExtensions() {
  return &g_widget_extensions;
}

PepperWidget::PepperWidget() : instance_(NULL), id_(0) {
}

PepperWidget::~PepperWidget() {
  if (id_)
    g_widgets.Get().erase(id_);
}

void PepperWidget::Init(NPP instance, int id) {
  instance_ = instance;
  id_ = id;
  g_widgets.Get()[id] = this;
}

void PepperWidget::WidgetPropertyChanged(NPWidgetProperty property) {
  NPAPI::PluginInstance* instance =
      static_cast<NPAPI::PluginInstance*>(instance_->ndata);
  NPPExtensions* extensions = NULL;
  instance->NPP_GetValue(NPPVPepperExtensions, &extensions);
  if (!extensions)
    return;

  extensions->widgetPropertyChanged(instance_, id_, property);
}
