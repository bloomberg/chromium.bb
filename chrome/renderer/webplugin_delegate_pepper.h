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
#include "base/linked_ptr.h"
#include "base/gfx/rect.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/common/transport_dib.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/npapi/bindings/npapi.h"
#include "webkit/glue/pepper/pepper.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webplugin_delegate.h"

namespace NPAPI {
class PluginInstance;
}

// An implementation of WebPluginDelegate for Pepper in-process plugins.
class WebPluginDelegatePepper : public webkit_glue::WebPluginDelegate {
 public:
  static WebPluginDelegatePepper* Create(const FilePath& filename,
      const std::string& mime_type, gfx::PluginWindowHandle containing_view);

  static bool IsPluginDelegateWindow(gfx::NativeWindow window);
  static bool GetPluginNameFromWindow(gfx::NativeWindow window,
                                      std::wstring *plugin_name);

  // Returns true if the window handle passed in is that of the dummy
  // activation window for windowless plugins.
  static bool IsDummyActivationWindow(gfx::NativeWindow window);

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
      int resource_id,
      const GURL& url,
      bool notify_needed,
      intptr_t notify_data,
      intptr_t stream);
  // End of WebPluginDelegate implementation.

  bool IsWindowless() const { return true; }
  gfx::Rect GetRect() const { return window_rect_; }
  gfx::Rect GetClipRect() const { return clip_rect_; }

  // Returns the path for the library implementing this plugin.
  FilePath GetPluginPath();

 private:
  WebPluginDelegatePepper(gfx::PluginWindowHandle containing_view,
                          NPAPI::PluginInstance *instance);
  ~WebPluginDelegatePepper();

  //----------------------------
  // used for windowless plugins
  virtual NPError InitializeRenderContext(NPRenderType type,
                                          NPRenderContext* context);
  virtual NPError DestroyRenderContext(NPRenderContext* context);
  virtual NPError FlushRenderContext(NPRenderContext* context);

  virtual NPError OpenFileInSandbox(const char* file_name, void** handle);

  // Tells the plugin about the current state of the window.
  // See NPAPI NPP_SetWindow for more information.
  void WindowlessSetWindow(bool force_set_window);

  //-----------------------------------------
  // used for windowed and windowless plugins

  NPAPI::PluginInstance* instance() { return instance_.get(); }

  // Closes down and destroys our plugin instance.
  void DestroyInstance();

  webkit_glue::WebPlugin* plugin_;
  scoped_refptr<NPAPI::PluginInstance> instance_;

  gfx::PluginWindowHandle parent_;
  NPWindow window_;
  gfx::Rect window_rect_;
  gfx::Rect clip_rect_;
  std::vector<gfx::Rect> cutout_rects_;

  // Plugin graphics context implementation
  size_t buffer_size_;
  TransportDIB* plugin_buffer_;
  static uint32 next_buffer_id;
  SkBitmap committed_bitmap_;

  // Lists all contexts currently open for painting. These are ones requested by
  // the plugin but not destroyed by it yet. The source pointer is the raw
  // pixels. We use this to look up the corresponding transport DIB when the
  // plugin tells us to flush or destroy it.
  struct OpenPaintContext {
    scoped_ptr<TransportDIB> transport_dib;

    // The canvas associated with the transport DIB, containing the mapped
    // memory of the image.
    scoped_ptr<skia::PlatformCanvas> canvas;
  };
  typedef std::map< void*, linked_ptr<OpenPaintContext> > OpenPaintContextMap;
  OpenPaintContextMap open_paint_contexts_;

  // The url with which the plugin was instantiated.
  std::string plugin_url_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginDelegatePepper);
};

#endif  // CHROME_RENDERER_WEBPLUGIN_DELEGATE_PEPPER_H_
