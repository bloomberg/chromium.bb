// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBPLUGIN_DELEGATE_PEPPER_H_
#define CHROME_RENDERER_WEBPLUGIN_DELEGATE_PEPPER_H_
#pragma once

#include "build/build_config.h"

#include <string>
#include <vector>

#include "base/id_map.h"
#include "base/ref_counted.h"
#include "base/weak_ptr.h"
#include "base/task.h"
#include "chrome/renderer/pepper_devices.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserCompletion.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/npapi/webplugin_delegate.h"

class FilePath;
class RenderView;
class WebCursor;
class WebPluginDelegateProxy;

namespace webkit {
namespace npapi {
class PluginInstance;
}
}

// An implementation of WebPluginDelegate for Pepper in-process plugins.
class WebPluginDelegatePepper : public webkit::npapi::WebPluginDelegate,
                                public WebKit::WebFileChooserCompletion {
 public:
  static WebPluginDelegatePepper* Create(
      const FilePath& filename,
      const std::string& mime_type,
      const base::WeakPtr<RenderView>& render_view);

  webkit::npapi::PluginInstance* instance() { return instance_.get(); }

  // WebKit::WebFileChooserCompletion implementation.
  virtual void didChooseFile(
      const WebKit::WebVector<WebKit::WebString>& file_names);

  // WebPluginDelegate implementation
  virtual bool Initialize(const GURL& url,
                          const std::vector<std::string>& arg_names,
                          const std::vector<std::string>& arg_values,
                          webkit::npapi::WebPlugin* plugin,
                          bool load_manually);
  virtual void PluginDestroyed();
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect);
  virtual void Paint(WebKit::WebCanvas* canvas, const gfx::Rect& rect);
  virtual void Print(gfx::NativeDrawingContext context);
  virtual void SetFocus(bool focused);
  virtual bool HandleInputEvent(const WebKit::WebInputEvent& event,
                                WebKit::WebCursorInfo* cursor);
  virtual NPObject* GetPluginScriptableObject();
  virtual void DidFinishLoadWithReason(const GURL& url, NPReason reason,
                                       int notify_id);
  virtual int GetProcessId();
  virtual void SendJavaScriptStream(const GURL& url,
                                    const std::string& result,
                                    bool success,
                                    int notify_id);
  virtual void DidReceiveManualResponse(const GURL& url,
                                        const std::string& mime_type,
                                        const std::string& headers,
                                        uint32 expected_length,
                                        uint32 last_modified);
  virtual void DidReceiveManualData(const char* buffer, int length);
  virtual void DidFinishManualLoading();
  virtual void DidManualLoadFail();
  virtual void InstallMissingPlugin();
  virtual webkit::npapi::WebPluginResourceClient* CreateResourceClient(
      unsigned long resource_id, const GURL& url, int notify_id);
  virtual webkit::npapi::WebPluginResourceClient* CreateSeekableResourceClient(
      unsigned long resource_id, int range_request_id);
  virtual bool StartFind(const string16& search_text,
                         bool case_sensitive,
                         int identifier);
  virtual void SelectFindResult(bool forward);
  virtual void StopFind();
  virtual void NumberOfFindResultsChanged(int total, bool final_result);
  virtual void SelectedFindResultChanged(int index);
  virtual bool ChooseFile(const char* mime_types,
                          int mode,
                          NPChooseFileCallback callback,
                          void* user_data);
  virtual NPWidgetExtensions* GetWidgetExtensions();
  virtual bool SetCursor(NPCursorType type);
  virtual NPFontExtensions* GetFontExtensions();
  virtual void SetZoomFactor(float scale, bool text_only);
  virtual bool HasSelection() const;
  virtual string16 GetSelectionAsText() const;
  virtual string16 GetSelectionAsMarkup() const;

  // WebPlugin2DDeviceDelegate implementation.
  virtual NPError Device2DQueryCapability(int32 capability, int32* value);
  virtual NPError Device2DQueryConfig(const NPDeviceContext2DConfig* request,
                                      NPDeviceContext2DConfig* obtain);
  virtual NPError Device2DInitializeContext(
      const NPDeviceContext2DConfig* config,
      NPDeviceContext2D* context);
  virtual NPError Device2DSetStateContext(NPDeviceContext2D* context,
                                          int32 state,
                                          intptr_t value);
  virtual NPError Device2DGetStateContext(NPDeviceContext2D* context,
                                          int32 state,
                                          intptr_t* value);
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
                                          intptr_t value);
  virtual NPError Device3DGetStateContext(NPDeviceContext3D* context,
                                          int32 state,
                                          intptr_t* value);
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
  virtual NPError Device3DGetNumConfigs(int32* num_configs);
  virtual NPError Device3DGetConfigAttribs(int32 config,
                                           int32* attrib_list);
  virtual NPError Device3DCreateContext(int32 config,
                                        const int32* attrib_list,
                                        NPDeviceContext3D** context);
  virtual NPError Device3DRegisterCallback(
      NPP id,
      NPDeviceContext3D* context,
      int32 callback_type,
      NPDeviceGenericCallbackPtr callback,
      void* callback_data);
  virtual NPError Device3DSynchronizeContext(
      NPP id,
      NPDeviceContext3D* context,
      NPDeviceSynchronizationMode mode,
      const int32* input_attrib_list,
      int32* output_attrib_list,
      NPDeviceSynchronizeContextCallbackPtr callback,
      void* callback_data);

  // WebPluginAudioDeviceDelegate implementation.
  virtual NPError DeviceAudioQueryCapability(int32 capability, int32* value);
  virtual NPError DeviceAudioQueryConfig(
      const NPDeviceContextAudioConfig* request,
      NPDeviceContextAudioConfig* obtain);
  virtual NPError DeviceAudioInitializeContext(
      const NPDeviceContextAudioConfig* config,
      NPDeviceContextAudio* context);
  virtual NPError DeviceAudioSetStateContext(NPDeviceContextAudio* context,
                                             int32 state, intptr_t value);
  virtual NPError DeviceAudioGetStateContext(NPDeviceContextAudio* context,
                                             int32 state, intptr_t* value);
  virtual NPError DeviceAudioFlushContext(
      NPP id, NPDeviceContextAudio* context,
      NPDeviceFlushContextCallbackPtr callback, void* user_data);
  virtual NPError DeviceAudioDestroyContext(NPDeviceContextAudio* context);

  // WebPluginPrintDelegate implementation.
  virtual bool PrintSupportsPrintExtension();
  virtual int PrintBegin(const gfx::Rect& printable_area, int printer_dpi);
  virtual bool PrintPage(int page_number, WebKit::WebCanvas* canvas);
  virtual void PrintEnd();

  gfx::Rect GetRect() const { return window_rect_; }
  gfx::Rect GetClipRect() const { return clip_rect_; }

  // Returns the path for the library implementing this plugin.
  FilePath GetPluginPath();

  // Notifications when the RenderView painted the screen (InitiatedPaint) and
  // when an ack was received that the browser copied it to the screen
  // (FlushedPaint).
  void RenderViewInitiatedPaint();
  void RenderViewFlushedPaint();

  Graphics2DDeviceContext* GetGraphicsContext(NPDeviceContext2D* context);

 private:
  WebPluginDelegatePepper(
      const base::WeakPtr<RenderView>& render_view,
      webkit::npapi::PluginInstance *instance);
  ~WebPluginDelegatePepper();

  //-----------------------------------------
  // used for windowed and windowless plugins

  // Closes down and destroys our plugin instance.
  void DestroyInstance();

  void ForwardSetWindow();

  // A helper method that invokes the plugin's Print extensions to calculate
  // the size needed in pixels to render the given page in a raster format.
  bool CalculatePrintedPageDimensions(int page_number,
                                      NPPPrintExtensions* print_extensions,
                                      gfx::Size* page_dimensions);
  bool VectorPrintPage(int page_number, WebKit::WebCanvas* canvas);

  NPPPrintExtensions* GetPrintExtensions();

  NPPFindExtensions* GetFindExtensions();

#if defined(OS_WIN)
  // Compresses the given bitmap as JPEG and draws it into the backing platform
  // DC (Windows-only).
  bool DrawJPEGToPlatformDC(const SkBitmap& bitmap,
                            const gfx::Rect& printable_area,
                            WebKit::WebCanvas* canvas);
#endif  // OS_WIN
#if defined(OS_MACOSX)
  // Draws the given kARGB_8888_Config bitmap to the specified canvas starting
  // at the specified destination rect.
  void DrawSkBitmapToCanvas(const SkBitmap& bitmap, WebKit::WebCanvas* canvas,
                            const gfx::Rect& dest_rect, int canvas_height);
#endif  // OS_MACOSX

  // Returns the selection.  If nothing is selected, returns an empty string.
  // If html is true, it will return a string only if html data is available.
  string16 GetSelectedText(bool html) const;

  base::WeakPtr<RenderView> render_view_;

  webkit::npapi::WebPlugin* plugin_;
  scoped_refptr<webkit::npapi::PluginInstance> instance_;

  NPWindow window_;
  gfx::Rect window_rect_;
  gfx::Rect clip_rect_;
  std::vector<gfx::Rect> cutout_rects_;

  // Open device contexts
  typedef IDMap<Graphics2DDeviceContext, IDMapOwnPointer> Graphics2DMap;
  Graphics2DMap graphic2d_contexts_;
  IDMap<AudioDeviceContext, IDMapOwnPointer> audio_contexts_;

  // Plugin graphics context implementation
  SkBitmap committed_bitmap_;

  // The url with which the plugin was instantiated.
  std::string plugin_url_;

  // The last printable_area passed in to PrintBegin. We remember this because
  // we need to stretch the printed raster bitmap to these dimensions. It is
  // cleared in PrintEnd.
  gfx::Rect current_printable_area_;
  int current_printer_dpi_;
#if defined(OS_MACOSX)
  // On the Mac, when we draw the bitmap to the PDFContext, it seems necessary
  // to keep the pixels valid until CGContextEndPage is called. We use this
  // variable to hold on to the pixels.
  SkBitmap last_printed_page_;
#endif   // defined(OS_MACOSX)
#if defined (OS_LINUX)
  // On Linux, we always send all pages from the renderer to the browser.
  // So, if the plugin supports printPagesAsPDF we print the entire output
  // in one shot in the first call to PrintPage.
  // (This is a temporary hack until we change the WebFrame and WebPlugin print
  // interfaces).
  // Specifies the total number of pages to be printed. It it set in PrintBegin.
  int32 num_pages_;
  // Specifies whether we have already output all pages. This is used to ignore
  // subsequent PrintPage requests.
  bool pdf_output_done_;
#endif   // defined(OS_LINUX)

  // The id of the current find operation, or -1 if none is in process.
  int find_identifier_;

  // When a choose file operation is outstanding, this will contain a
  // pointer to the callback specified by the plugin. Will be NULL otherwise.
  NPChooseFileCallback current_choose_file_callback_;
  void* current_choose_file_user_data_;

  scoped_ptr<WebKit::WebCursorInfo> cursor_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginDelegatePepper);
};

#endif  // CHROME_RENDERER_WEBPLUGIN_DELEGATE_PEPPER_H_
