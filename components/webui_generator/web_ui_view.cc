// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webui_generator/web_ui_view.h"

#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "components/login/localized_values_builder.h"
#include "components/webui_generator/data_source_util.h"
#include "components/webui_generator/view_model.h"

namespace {

const char kEventCallback[] = "eventFired";
const char kContextChangedCallback[] = "contextChanged";
const char kReadyCallback[] = "ready";

const char kMethodContextChanged[] = "contextChanged";

}  // namespace

namespace webui_generator {

WebUIView::WebUIView(content::WebUI* web_ui, const std::string& id)
    : View(id), web_ui_(web_ui), html_ready_(false), view_bound_(false) {
}

WebUIView::~WebUIView() {
}

void WebUIView::Init() {
  View::Init();
  AddCallback(path() + "$" + kEventCallback, &WebUIView::HandleEvent);
  AddCallback(path() + "$" + kContextChangedCallback,
              &WebUIView::HandleContextChanged);
  AddCallback(path() + "$" + kReadyCallback, &WebUIView::HandleHTMLReady);
}

void WebUIView::SetUpDataSource(content::WebUIDataSource* data_source) {
  DCHECK(IsRootView());
  webui_generator::SetUpDataSource(data_source);
  ResourcesMap* resources_map = new ResourcesMap();
  SetUpDataSourceInternal(data_source, resources_map);
  data_source->SetRequestFilter(base::Bind(&WebUIView::HandleDataRequest,
                                           base::Unretained(this),
                                           base::Owned(resources_map)));
}

void WebUIView::OnReady() {
  View::OnReady();
  if (html_ready_)
    Bind();
}

void WebUIView::OnContextChanged(const base::DictionaryValue& diff) {
  if (view_bound_)
    web_ui_->CallJavascriptFunction(path() + "$" + kMethodContextChanged, diff);

  if (!pending_context_changes_)
    pending_context_changes_.reset(new Context());

  pending_context_changes_->ApplyChanges(diff, nullptr);
}

void WebUIView::SetUpDataSourceInternal(content::WebUIDataSource* data_source,
                                        ResourcesMap* resources_map) {
  base::DictionaryValue strings;
  auto builder = make_scoped_ptr(
      new ::login::LocalizedValuesBuilder(GetType() + "$", &strings));
  AddLocalizedValues(builder.get());
  data_source->AddLocalizedStrings(strings);
  AddResources(resources_map);
  for (auto& id_child : children()) {
    static_cast<WebUIView*>(id_child.second)
        ->SetUpDataSourceInternal(data_source, resources_map);
  }
}

bool WebUIView::HandleDataRequest(
    const ResourcesMap* resources,
    const std::string& path,
    const content::WebUIDataSource::GotDataCallback& got_data_callback) {
  auto it = resources->find(path);

  if (it == resources->end())
    return false;

  got_data_callback.Run(it->second.get());
  return true;
}

void WebUIView::HandleHTMLReady() {
  html_ready_ = true;
  if (ready())
    Bind();
}

void WebUIView::HandleContextChanged(const base::DictionaryValue* diff) {
  UpdateContext(*diff);
}

void WebUIView::Bind() {
  view_bound_ = true;
  if (pending_context_changes_)
    OnContextChanged(pending_context_changes_->storage());

  GetViewModel()->OnViewBound();
}

}  // namespace webui_generator
