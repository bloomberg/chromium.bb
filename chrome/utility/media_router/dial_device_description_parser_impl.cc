// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_router/dial_device_description_parser_impl.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace {

// If friendly name does not exist, fall back to use model name + last 4
// digits of UUID as friendly name.
std::string ComputeFriendlyName(const std::string& unique_id,
                                const std::string& model_name) {
  if (model_name.empty() || unique_id.length() < 4)
    return std::string();

  std::string trimmed_unique_id = unique_id.substr(unique_id.length() - 4);
  return base::StringPrintf("%s [%s]", model_name.c_str(),
                            trimmed_unique_id.c_str());
}

}  // namespace

namespace media_router {

DialDeviceDescriptionParserImpl::DialDeviceDescriptionParserImpl() = default;
DialDeviceDescriptionParserImpl::~DialDeviceDescriptionParserImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
void DialDeviceDescriptionParserImpl::Create(
    chrome::mojom::DialDeviceDescriptionParserRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<DialDeviceDescriptionParserImpl>(),
                          std::move(request));
}

void DialDeviceDescriptionParserImpl::ParseDialDeviceDescription(
    const std::string& device_description_xml_data,
    ParseDialDeviceDescriptionCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  chrome::mojom::DialParsingError parsing_error =
      chrome::mojom::DialParsingError::NONE;
  chrome::mojom::DialDeviceDescriptionPtr device_description =
      Parse(device_description_xml_data, &parsing_error);
  std::move(callback).Run(std::move(device_description), parsing_error);
}

chrome::mojom::DialDeviceDescriptionPtr DialDeviceDescriptionParserImpl::Parse(
    const std::string& xml,
    chrome::mojom::DialParsingError* parsing_error) {
  *parsing_error = chrome::mojom::DialParsingError::NONE;

  XmlReader xml_reader;
  if (!xml_reader.Load(xml)) {
    *parsing_error = chrome::mojom::DialParsingError::INVALID_XML;
    return nullptr;
  }

  chrome::mojom::DialDeviceDescriptionPtr out =
      chrome::mojom::DialDeviceDescription::New();

  while (xml_reader.Read()) {
    xml_reader.SkipToElement();
    std::string node_name(xml_reader.NodeName());

    if (node_name == "UDN") {
      if (!xml_reader.ReadElementContent(&out->unique_id)) {
        *parsing_error = chrome::mojom::DialParsingError::FAILED_TO_READ_UDN;
        return nullptr;
      }
    } else if (node_name == "friendlyName") {
      if (!xml_reader.ReadElementContent(&out->friendly_name)) {
        *parsing_error =
            chrome::mojom::DialParsingError::FAILED_TO_READ_FRIENDLY_NAME;
        return nullptr;
      }
    } else if (node_name == "modelName") {
      if (!xml_reader.ReadElementContent(&out->model_name)) {
        *parsing_error =
            chrome::mojom::DialParsingError::FAILED_TO_READ_MODEL_NAME;
        return nullptr;
      }
    } else if (node_name == "deviceType") {
      if (!xml_reader.ReadElementContent(&out->device_type)) {
        *parsing_error =
            chrome::mojom::DialParsingError::FAILED_TO_READ_DEVICE_TYPE;
        return nullptr;
      }
    }
  }

  if (out->friendly_name.empty())
    out->friendly_name = ComputeFriendlyName(out->unique_id, out->model_name);

  return out;
}

}  // namespace media_router
