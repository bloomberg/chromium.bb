// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/webplugin_delegate_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/child/npapi/plugin_instance.h"
#include "content/child/npapi/plugin_lib.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebCursorInfo;
using blink::WebInputEvent;

namespace content {

WebPluginDelegateImpl* WebPluginDelegateImpl::Create(
    WebPlugin* plugin,
    const base::FilePath& filename,
    const std::string& mime_type) {
  scoped_refptr<PluginLib> plugin_lib(PluginLib::CreatePluginLib(filename));
  if (plugin_lib.get() == NULL)
    return NULL;

  NPError err = plugin_lib->NP_Initialize();
  if (err != NPERR_NO_ERROR)
    return NULL;

  scoped_refptr<PluginInstance> instance(plugin_lib->CreateInstance(mime_type));
  return new WebPluginDelegateImpl(plugin, instance.get());
}

void WebPluginDelegateImpl::PluginDestroyed() {
  if (handle_event_depth_) {
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  } else {
    delete this;
  }
}

bool WebPluginDelegateImpl::Initialize(
    const GURL& url,
    const std::vector<std::string>& arg_names,
    const std::vector<std::string>& arg_values,
    bool load_manually) {
  if (instance_->plugin_lib()->plugin_info().name.find(
          base::ASCIIToUTF16("QuickTime Plug-in")) != std::wstring::npos) {
    quirks_ |= PLUGIN_QUIRK_COPY_STREAM_DATA;
  }

  instance_->set_web_plugin(plugin_);
  if (quirks_ & PLUGIN_QUIRK_DONT_ALLOW_MULTIPLE_INSTANCES) {
    PluginLib* plugin_lib = instance()->plugin_lib();
    if (plugin_lib->instance_count() > 1) {
      return false;
    }
  }

  int argc = 0;
  scoped_ptr<char*[]> argn(new char*[arg_names.size()]);
  scoped_ptr<char*[]> argv(new char*[arg_names.size()]);
  for (size_t i = 0; i < arg_names.size(); ++i) {
    if (quirks_ & PLUGIN_QUIRK_NO_WINDOWLESS &&
        base::LowerCaseEqualsASCII(arg_names[i], "windowlessvideo")) {
      continue;
    }
    argn[argc] = const_cast<char*>(arg_names[i].c_str());
    argv[argc] = const_cast<char*>(arg_values[i].c_str());
    argc++;
  }

  creation_succeeded_ = instance_->Start(
      url, argn.get(), argv.get(), argc, load_manually);
  if (!creation_succeeded_) {
    VLOG(1) << "Couldn't start plugin instance";
    return false;
  }

  windowless_ = instance_->windowless();
  if (!windowless_) {
    if (!WindowedCreatePlugin()) {
      VLOG(1) << "Couldn't create windowed plugin";
      return false;
    }
  }

  bool should_load = PlatformInitialize();

  plugin_url_ = url.spec();

  return should_load;
}

void WebPluginDelegateImpl::DestroyInstance() {
  if (instance_.get() && (instance_->npp()->ndata != NULL)) {
    window_.window = NULL;
    if (creation_succeeded_ &&
        !(quirks_ & PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY)) {
      instance_->NPP_SetWindow(&window_);
    }

    instance_->NPP_Destroy();

    instance_->set_web_plugin(NULL);

    PlatformDestroyInstance();

    instance_ = 0;
  }
}

void WebPluginDelegateImpl::UpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {

  if (first_set_window_call_) {
    first_set_window_call_ = false;
    // Plugins like media player on Windows have a bug where in they handle the
    // first geometry update and ignore the rest resulting in painting issues.
    // This quirk basically ignores the first set window call sequence for
    // these plugins and has been tested for Windows plugins only.
    if (quirks_ & PLUGIN_QUIRK_IGNORE_FIRST_SETWINDOW_CALL)
      return;
  }

  if (windowless_) {
    WindowlessUpdateGeometry(window_rect, clip_rect);
  } else {
    WindowedUpdateGeometry(window_rect, clip_rect);
  }
}

void WebPluginDelegateImpl::SetFocus(bool focused) {
  DCHECK(windowless_);
  // This is called when internal WebKit focus (the focused element on the page)
  // changes, but plugins need to know about OS-level focus, so we have an extra
  // layer of focus tracking.
  //
  // On Windows, historically browsers did not set focus events to windowless
  // plugins when the toplevel window focus changes. Sending such focus events
  // breaks full screen mode in Flash because it will come out of full screen
  // mode when it loses focus, and its full screen window causes the browser to
  // lose focus.
  has_webkit_focus_ = focused;
#if !defined(OS_WIN)
  if (containing_view_has_focus_)
    SetPluginHasFocus(focused);
#else
  SetPluginHasFocus(focused);
#endif
}

void WebPluginDelegateImpl::SetPluginHasFocus(bool focused) {
  if (focused == plugin_has_focus_)
    return;
  if (PlatformSetPluginHasFocus(focused))
    plugin_has_focus_ = focused;
}

void WebPluginDelegateImpl::SetContentAreaHasFocus(bool has_focus) {
  containing_view_has_focus_ = has_focus;
  if (!windowless_)
    return;
#if !defined(OS_WIN)  // See SetFocus above.
  SetPluginHasFocus(containing_view_has_focus_ && has_webkit_focus_);
#endif
}

NPObject* WebPluginDelegateImpl::GetPluginScriptableObject() {
  return instance_->GetPluginScriptableObject();
}

NPP WebPluginDelegateImpl::GetPluginNPP() {
  return instance_->npp();
}

bool WebPluginDelegateImpl::GetFormValue(base::string16* value) {
  return instance_->GetFormValue(value);
}

int WebPluginDelegateImpl::GetProcessId() {
  // We are in process, so the plugin pid is this current process pid.
  return base::GetCurrentProcId();
}

base::FilePath WebPluginDelegateImpl::GetPluginPath() {
  return instance()->plugin_lib()->plugin_info().path;
}

void WebPluginDelegateImpl::WindowedUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  if (WindowedReposition(window_rect, clip_rect) ||
      !windowed_did_set_window_) {
    // Let the plugin know that it has been moved
    WindowedSetWindow();
  }
}

bool WebPluginDelegateImpl::HandleInputEvent(
    const WebInputEvent& event,
    WebCursor::CursorInfo* cursor_info) {
  DCHECK(windowless_) << "events should only be received in windowless mode";

  bool pop_user_gesture = false;
  if (IsUserGesture(event)) {
    pop_user_gesture = true;
    instance()->PushPopupsEnabledState(true);
  }

  bool handled = PlatformHandleInputEvent(event, cursor_info);

  if (pop_user_gesture) {
    instance()->PopPopupsEnabledState();
  }

  return handled;
}

bool WebPluginDelegateImpl::IsUserGesture(const WebInputEvent& event) {
  switch (event.type) {
    case WebInputEvent::MouseDown:
    case WebInputEvent::MouseUp:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
      return true;
    default:
      return false;
  }
}

}  // namespace content
