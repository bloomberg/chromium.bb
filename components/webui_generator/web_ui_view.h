// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WUG_WEB_UI_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_WUG_WEB_UI_VIEW_H_

#include <string>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "components/login/base_screen_handler_utils.h"
#include "components/login/screens/screen_context.h"
#include "components/webui_generator/export.h"
#include "components/webui_generator/view.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace base {
class DictionaryValue;
}

namespace content {
class WebUI;
}

namespace login {
class LocalizedValuesBuilder;
}

namespace webui_generator {

using Context = login::ScreenContext;

/**
 * View implementation for the WebUI. Represents a single HTML element derieved
 * from <web-ui-view>.
 */
class WUG_EXPORT WebUIView : public View {
 public:
  WebUIView(content::WebUI* web_ui, const std::string& id);
  ~WebUIView() override;

  // Should be called for |data_source|, where this views will be used.
  // Sets up |data_source| for children of |this| as well, that is why this
  // method only shouldn't be called for non-root views.
  void SetUpDataSource(content::WebUIDataSource* data_source);

  // Overridden from View:
  void Init() override;

 protected:
  using ResourcesMap =
      base::hash_map<std::string, scoped_refptr<base::RefCountedMemory>>;

  content::WebUI* web_ui() { return web_ui_; }

  template <typename T>
  void AddCallback(const std::string& name, void (T::*method)()) {
    base::Callback<void()> callback =
        base::Bind(method, base::Unretained(static_cast<T*>(this)));
    web_ui_->RegisterMessageCallback(
        name, base::Bind(&::login::CallbackWrapper0, callback));
  }

  template <typename T, typename A1>
  void AddCallback(const std::string& name, void (T::*method)(A1 arg1)) {
    base::Callback<void(A1)> callback =
        base::Bind(method, base::Unretained(static_cast<T*>(this)));
    web_ui_->RegisterMessageCallback(
        name, base::Bind(&::login::CallbackWrapper1<A1>, callback));
  }

  // Overridden from View:
  void OnReady() override;
  void OnContextChanged(const base::DictionaryValue& diff) override;

  virtual void AddLocalizedValues(::login::LocalizedValuesBuilder* builder) = 0;
  virtual void AddResources(ResourcesMap* resources_map) = 0;

 private:
  // Internal implementation of SetUpDataSource.
  void SetUpDataSourceInternal(content::WebUIDataSource* data_source,
                               ResourcesMap* resources_map);

  // Called when corresponding <web-ui-view> is ready.
  void HandleHTMLReady();

  // Called when context is changed on JS side.
  void HandleContextChanged(const base::DictionaryValue* diff);

  // Called when both |this| and corresponding <web-ui-view> are ready.
  void Bind();

  // Request filter for data source. Filter is needed to answer with a generated
  // HTML and JS code which is not kept in resources.
  bool HandleDataRequest(
      const ResourcesMap* resources,
      const std::string& path,
      const content::WebUIDataSource::GotDataCallback& got_data_callback);

 private:
  content::WebUI* web_ui_;
  bool html_ready_;
  bool view_bound_;
  scoped_ptr<Context> pending_context_changes_;

  DISALLOW_COPY_AND_ASSIGN(WebUIView);
};

}  // namespace webui_generator

#endif  // CHROME_BROWSER_UI_WEBUI_WUG_WEB_UI_VIEW_H_
