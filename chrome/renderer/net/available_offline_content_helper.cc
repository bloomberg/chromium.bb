// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/available_offline_content_helper.h"

#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/common/available_offline_content.mojom.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

base::Value AvailableContentToValue(
    const chrome::mojom::AvailableOfflineContentPtr& content) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey("ID", base::Value(content->id));
  value.SetKey("name_space", base::Value(content->name_space));
  value.SetKey("title", base::Value(content->title));
  value.SetKey("snippet", base::Value(content->snippet));
  value.SetKey("date_modified", base::Value(content->date_modified));
  value.SetKey("attribution", base::Value(content->attribution));
  value.SetKey("thumbnail_data_uri",
               base::Value(content->thumbnail_data_uri.spec()));
  return value;
}

base::Value AvailableContentListToValue(
    const std::vector<chrome::mojom::AvailableOfflineContentPtr>&
        content_list) {
  base::Value value(base::Value::Type::LIST);
  for (const auto& content : content_list) {
    value.GetList().push_back(AvailableContentToValue(content));
  }
  return value;
}

void AvailableContentReceived(
    base::OnceCallback<void(const std::string& offline_content_json)> callback,
    std::vector<chrome::mojom::AvailableOfflineContentPtr> content) {
  std::string json;
  if (!content.empty()) {
    base::JSONWriter::Write(AvailableContentListToValue(content), &json);
  }
  std::move(callback).Run(json);
}

}  // namespace

AvailableOfflineContentHelper::AvailableOfflineContentHelper() = default;
AvailableOfflineContentHelper::~AvailableOfflineContentHelper() = default;

void AvailableOfflineContentHelper::Reset() {
  provider_.reset();
}

void AvailableOfflineContentHelper::FetchAvailableContent(
    base::OnceCallback<void(const std::string& offline_content_json)>
        callback) {
  if (!BindProvider()) {
    std::move(callback).Run({});
    return;
  }
  provider_->List(
      base::BindOnce(&AvailableContentReceived, std::move(callback)));
}

bool AvailableOfflineContentHelper::BindProvider() {
  if (provider_)
    return true;
  content::RenderThread::Get()->GetConnector()->BindInterface(
      content::mojom::kBrowserServiceName, &provider_);
  return !!provider_;
}
