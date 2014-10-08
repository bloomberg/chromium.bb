// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_FAKE_PEPPER_PLUGIN_INSTANCE_H_
#define CONTENT_RENDERER_PEPPER_FAKE_PEPPER_PLUGIN_INSTANCE_H_

#include "content/public/renderer/pepper_plugin_instance.h"
#include "url/gurl.h"

namespace content {

class FakePepperPluginInstance : public PepperPluginInstance {
 public:
  virtual ~FakePepperPluginInstance();

  // PepperPluginInstance overrides.
  virtual content::RenderView* GetRenderView() override;
  virtual blink::WebPluginContainer* GetContainer() override;
  virtual v8::Isolate* GetIsolate() const override;
  virtual ppapi::VarTracker* GetVarTracker() override;
  virtual const GURL& GetPluginURL() override;
  virtual base::FilePath GetModulePath() override;
  virtual PP_Resource CreateImage(gfx::ImageSkia* source_image,
                                  float scale) override;
  virtual PP_ExternalPluginResult SwitchToOutOfProcessProxy(
      const base::FilePath& file_path,
      ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id) override;
  virtual void SetAlwaysOnTop(bool on_top) override;
  virtual bool IsFullPagePlugin() override;
  virtual bool FlashSetFullscreen(bool fullscreen, bool delay_report) override;
  virtual bool IsRectTopmost(const gfx::Rect& rect) override;
  virtual int32_t Navigate(const ppapi::URLRequestInfoData& request,
                           const char* target,
                           bool from_user_action) override;
  virtual int MakePendingFileRefRendererHost(const base::FilePath& path)
      override;
  virtual void SetEmbedProperty(PP_Var key, PP_Var value) override;
  virtual void SetSelectedText(const base::string16& selected_text) override;
  virtual void SetLinkUnderCursor(const std::string& url) override;
  virtual void SetTextInputType(ui::TextInputType type) override;
  virtual void PostMessageToJavaScript(PP_Var message) override;

 private:
  GURL gurl_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_FAKE_PEPPER_PLUGIN_INSTANCE_H_
