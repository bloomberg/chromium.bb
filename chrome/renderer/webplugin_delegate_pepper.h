// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBPLUGIN_DELEGATE_PEPPER_H_
#define CHROME_RENDERER_WEBPLUGIN_DELEGATE_PEPPER_H_

#include "build/build_config.h"

#include <map>
#include <string>
#include <vector>

#include "app/gfx/native_widget_types.h"
#include "base/file_path.h"
#include "base/id_map.h"
#include "base/gfx/rect.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "base/weak_ptr.h"
#include "chrome/renderer/pepper_devices.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/command_buffer_proxy.h"
#include "third_party/npapi/bindings/npapi.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webplugin_delegate.h"

namespace NPAPI {
class PluginInstance;
}

// An implementation of WebPluginDelegate for Pepper in-process plugins.
class WebPluginDelegatePepper : public webkit_glue::WebPluginDelegate {
 public:
  static WebPluginDelegatePepper* Create(
      const FilePath& filename,
      const std::string& mime_type,
      const base::WeakPtr<RenderView>& render_view);

  // WebPluginDelegate implementation
  virtual bool Initialize(const GURL& url,
                          const std::vector<std::string>& arg_names,
                          const std::vector<std::string>& arg_values,
                          webkit_glue::WebPlugin* plugin,
                          bool load_manually);
  virtual void PluginDestroyed();
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect);
  virtual void Paint(WebKit::WebCanvas* canvas, const gfx::Rect& rect);
  virtual void Print(gfx::NativeDrawingContext context);
  virtual void SetFocus();
  virtual bool HandleInputEvent(const WebKit::WebInputEvent& event,
                                WebKit::WebCursorInfo* cursor);
  virtual NPObject* GetPluginScriptableObject();
  virtual void DidFinishLoadWithReason(const GURL& url, NPReason reason,
                                       intptr_t notify_data);
  virtual int GetProcessId();
  virtual void SendJavaScriptStream(const GURL& url,
                                    const std::string& result,
                                    bool success, bool notify_needed,
                                    intptr_t notify_data);
  virtual void DidReceiveManualResponse(const GURL& url,
                                        const std::string& mime_type,
                                        const std::string& headers,
                                        uint32 expected_length,
                                        uint32 last_modified);
  virtual void DidReceiveManualData(const char* buffer, int length);
  virtual void DidFinishManualLoading();
  virtual void DidManualLoadFail();
  virtual void InstallMissingPlugin();
  virtual webkit_glue::WebPluginResourceClient* CreateResourceClient(
      unsigned long resource_id,
      const GURL& url,
      bool notify_needed,
      intptr_t notify_data,
      intptr_t stream);

  // WebPlugin2DDeviceDelegate implementation.
  virtual NPError Device2DQueryCapability(int32 capability, int32* value);
  virtual NPError Device2DQueryConfig(const NPDeviceContext2DConfig* request,
                                      NPDeviceContext2DConfig* obtain);
  virtual NPError Device2DInitializeContext(
      const NPDeviceContext2DConfig* config,
      NPDeviceContext2D* context);
  virtual NPError Device2DSetStateContext(NPDeviceContext2D* context,
                                          int32 state,
                                          int32 value);
  virtual NPError Device2DGetStateContext(NPDeviceContext2D* context,
                                          int32 state,
                                          int32* value);
  virtual NPError Device2DFlushContext(NPP id,
                                       NPDeviceContext2D* context,
                                       NPDeviceFlushContextCallbackPtr callback,
                                       void* user_data);
  virtual NPError Device2DDestroyContext(NPDeviceContext2D* context);

  // WebPlugin3DDeviceDelegate implementation.
  virtual NPError Device3DQueryCapability(int32 capability, int32* value);
  virtual NPError Device3DQueryConfig(const NPDeviceContext3DConfig* request,
                                      NPDeviceContext3DConfig* obtain);
  virtual NPError Device3DInitializeContext(
      const NPDeviceContext3DConfig* config,
      NPDeviceContext3D* context);
  virtual NPError Device3DSetStateContext(NPDeviceContext3D* context,
                                          int32 state,
                                          int32 value);
  virtual NPError Device3DGetStateContext(NPDeviceContext3D* context,
                                          int32 state,
                                          int32* value);
  virtual NPError Device3DFlushContext(NPP id,
                                       NPDeviceContext3D* context,
                                       NPDeviceFlushContextCallbackPtr callback,
                                       void* user_data);
  virtual NPError Device3DDestroyContext(NPDeviceContext3D* context);
  virtual NPError Device3DCreateBuffer(NPDeviceContext3D* context,
                                       size_t size,
                                       int32* id);
  virtual NPError Device3DDestroyBuffer(NPDeviceContext3D* context,
                                        int32 id);
  virtual NPError Device3DMapBuffer(NPDeviceContext3D* context,
                                    int32 id,
                                    NPDeviceBuffer* buffer);

  // WebPluginAudioDeviceDelegate implementation.
  virtual NPError DeviceAudioQueryCapability(int32 capability, int32* value);
  virtual NPError DeviceAudioQueryConfig(
      const NPDeviceContextAudioConfig* request,
      NPDeviceContextAudioConfig* obtain);
  virtual NPError DeviceAudioInitializeContext(
      const NPDeviceContextAudioConfig* config,
      NPDeviceContextAudio* context);
  virtual NPError DeviceAudioSetStateContext(NPDeviceContextAudio* context,
                                             int32 state, int32 value);
  virtual NPError DeviceAudioGetStateContext(NPDeviceContextAudio* context,
                                             int32 state, int32* value);
  virtual NPError DeviceAudioFlushContext(
      NPP id, NPDeviceContextAudio* context,
      NPDeviceFlushContextCallbackPtr callback, void* user_data);
  virtual NPError DeviceAudioDestroyContext(NPDeviceContextAudio* context);

  // End of WebPluginDelegate implementation.

  gfx::Rect GetRect() const { return window_rect_; }
  gfx::Rect GetClipRect() const { return clip_rect_; }

  // Returns the path for the library implementing this plugin.
  FilePath GetPluginPath();

 private:
  WebPluginDelegatePepper(
      const base::WeakPtr<RenderView>& render_view,
      NPAPI::PluginInstance *instance);
  ~WebPluginDelegatePepper();

  // Tells the plugin about the current state of the window.
  // See NPAPI NPP_SetWindow for more information.
  void WindowlessSetWindow(bool force_set_window);

  //-----------------------------------------
  // used for windowed and windowless plugins

  NPAPI::PluginInstance* instance() { return instance_.get(); }

  // Closes down and destroys our plugin instance.
  void DestroyInstance();

  base::WeakPtr<RenderView> render_view_;

  webkit_glue::WebPlugin* plugin_;
  scoped_refptr<NPAPI::PluginInstance> instance_;

  NPWindow window_;
  gfx::Rect window_rect_;
  gfx::Rect clip_rect_;
  std::vector<gfx::Rect> cutout_rects_;

  // Open device contexts
  IDMap<Graphics2DDeviceContext, IDMapOwnPointer> graphic2d_contexts_;
  IDMap<AudioDeviceContext, IDMapOwnPointer> audio_contexts_;

  // Plugin graphics context implementation
  SkBitmap committed_bitmap_;

  // The url with which the plugin was instantiated.
  std::string plugin_url_;

  // The nested GPU plugin and its command buffer proxy.
  WebPluginDelegateProxy* nested_delegate_;
#if defined(ENABLE_GPU)
  scoped_ptr<CommandBufferProxy> command_buffer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WebPluginDelegatePepper);
};

#endif  // CHROME_RENDERER_WEBPLUGIN_DELEGATE_PEPPER_H_
