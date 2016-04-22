// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/logging.h"

#include <iostream>
#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/json/string_escape.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "blimp/common/proto/blimp_message.pb.h"

namespace blimp {
namespace {

static base::LazyInstance<BlimpMessageLogger> g_logger =
    LAZY_INSTANCE_INITIALIZER;

// The AddField() suite of functions are used to convert KV pairs with
// arbitrarily typed values into string/string KV pairs for logging.

// Specialization for string values, surrounding them with quotes and escaping
// characters as necessary.
void AddField(const std::string& key,
              const std::string& value,
              LogFields* output) {
  std::string escaped_value;
  base::EscapeJSONString(value, true, &escaped_value);
  output->push_back(std::make_pair(key, escaped_value));
}

// Specialization for string literal values.
void AddField(const std::string& key, const char* value, LogFields* output) {
  output->push_back(std::make_pair(key, std::string(value)));
}

// Specialization for boolean values (serialized as "true" or "false").
void AddField(const std::string& key, bool value, LogFields* output) {
  output->push_back(std::make_pair(key, (value ? "true" : "false")));
}

// Specialization for SizeMessage values, serializing them as
// WIDTHxHEIGHT:RATIO. RATIO is rounded to two digits of precision
// (e.g. 2.123 => 2.12).
void AddField(const std::string& key,
              const SizeMessage& value,
              LogFields* output) {
  output->push_back(std::make_pair(
      key, base::StringPrintf("%" PRIu64 "x%" PRIu64 ":%.2lf", value.width(),
                              value.height(), value.device_pixel_ratio())));
}

// Conversion function for all other types.
// Uses std::to_string() to serialize |value|.
template <typename T>
void AddField(const std::string& key, const T& value, LogFields* output) {
  output->push_back(std::make_pair(key, std::to_string(value)));
}

// The following LogExtractor subclasses contain logic for extracting loggable
// fields from BlimpMessages.

// Logs fields from COMPOSITOR messages.
class CompositorLogExtractor : public LogExtractor {
  void ExtractFields(const BlimpMessage& message,
                     LogFields* output) const override {
    AddField("render_widget_id", message.compositor().render_widget_id(),
             output);
  }
};

// Logs fields from INPUT messages.
class InputLogExtractor : public LogExtractor {
  void ExtractFields(const BlimpMessage& message,
                     LogFields* output) const override {
    AddField("render_widget_id", message.input().render_widget_id(), output);
    AddField("timestamp_seconds", message.input().timestamp_seconds(), output);
    switch (message.input().type()) {
      case InputMessage::Type_GestureScrollBegin:
        AddField("subtype", "GestureScrollBegin", output);
        break;
      case InputMessage::Type_GestureScrollEnd:
        AddField("subtype", "GestureScrollEnd", output);
        break;
      case InputMessage::Type_GestureScrollUpdate:
        AddField("subtype", "GestureScrollUpdate", output);
        break;
      case InputMessage::Type_GestureFlingStart:
        AddField("subtype", "GestureFlingStart", output);
        break;
      case InputMessage::Type_GestureFlingCancel:
        AddField("subtype", "GestureFlingCancel", output);
        AddField("prevent_boosting",
                 message.input().gesture_fling_cancel().prevent_boosting(),
                 output);
        break;
      case InputMessage::Type_GestureTap:
        AddField("subtype", "GestureTap", output);
        break;
      case InputMessage::Type_GesturePinchBegin:
        AddField("subtype", "GesturePinchBegin", output);
        break;
      case InputMessage::Type_GesturePinchEnd:
        AddField("subtype", "GesturePinchEnd", output);
        break;
      case InputMessage::Type_GesturePinchUpdate:
        AddField("subtype", "GesturePinchUpdate", output);
        break;
      default:  // unknown
        break;
    }
  }
};

// Logs fields from NAVIGATION messages.
class NavigationLogExtractor : public LogExtractor {
  void ExtractFields(const BlimpMessage& message,
                     LogFields* output) const override {
    switch (message.navigation().type()) {
      case NavigationMessage::NAVIGATION_STATE_CHANGED:
        AddField("subtype", "NAVIGATION_STATE_CHANGED", output);
        if (message.navigation().navigation_state_changed().has_url()) {
          AddField("url", message.navigation().navigation_state_changed().url(),
                   output);
        }
        if (message.navigation().navigation_state_changed().has_favicon()) {
          AddField(
              "favicon_size",
              message.navigation().navigation_state_changed().favicon().size(),
              output);
        }
        if (message.navigation().navigation_state_changed().has_title()) {
          AddField("title",
                   message.navigation().navigation_state_changed().title(),
                   output);
        }
        if (message.navigation().navigation_state_changed().has_loading()) {
          AddField("loading",
                   message.navigation().navigation_state_changed().loading(),
                   output);
        }
        break;
      case NavigationMessage::LOAD_URL:
        AddField("subtype", "LOAD_URL", output);
        AddField("url", message.navigation().load_url().url(), output);
        break;
      case NavigationMessage::GO_BACK:
        AddField("subtype", "GO_BACK", output);
        break;
      case NavigationMessage::GO_FORWARD:
        AddField("subtype", "GO_FORWARD", output);
        break;
      case NavigationMessage::RELOAD:
        AddField("subtype", "RELOAD", output);
        break;
      default:
        break;
    }
  }
};

// Logs fields from PROTOCOL_CONTROL messages.
class ProtocolControlLogExtractor : public LogExtractor {
  void ExtractFields(const BlimpMessage& message,
                     LogFields* output) const override {
    switch (message.protocol_control().type()) {
      case ProtocolControlMessage::START_CONNECTION:
        AddField("subtype", "START_CONNECTION", output);
        AddField("client_token",
                 message.protocol_control().start_connection().client_token(),
                 output);
        AddField(
            "protocol_version",
            message.protocol_control().start_connection().protocol_version(),
            output);
        break;
      case ProtocolControlMessage::CHECKPOINT_ACK:
        AddField("subtype", "CHECKPOINT_ACK", output);
        AddField("checkpoint_id",
                 message.protocol_control().checkpoint_ack().checkpoint_id(),
                 output);
        break;
      default:
        break;
    }
  }
};

// Logs fields from RENDER_WIDGET messages.
class RenderWidgetLogExtractor : public LogExtractor {
  void ExtractFields(const BlimpMessage& message,
                     LogFields* output) const override {
    switch (message.render_widget().type()) {
      case RenderWidgetMessage::INITIALIZE:
        AddField("subtype", "INITIALIZE", output);
        break;
      case RenderWidgetMessage::CREATED:
        AddField("subtype", "CREATED", output);
        break;
      case RenderWidgetMessage::DELETED:
        AddField("subtype", "DELETED", output);
        break;
    }
    AddField("render_widget_id", message.render_widget().render_widget_id(),
             output);
  }
};

// Logs fields from SETTINGS messages.
class SettingsLogExtractor : public LogExtractor {
  void ExtractFields(const BlimpMessage& message,
                     LogFields* output) const override {
    if (message.settings().has_engine_settings()) {
      const EngineSettingsMessage& engine_settings =
          message.settings().engine_settings();
      AddField("subtype", "ENGINE_SETTINGS", output);
      AddField("record_whole_document", engine_settings.record_whole_document(),
               output);
      AddField("client_os_info", engine_settings.client_os_info(), output);
    }
  }
};

// Logs fields from TAB_CONTROL messages.
class TabControlLogExtractor : public LogExtractor {
  void ExtractFields(const BlimpMessage& message,
                     LogFields* output) const override {
    switch (message.tab_control().type()) {
      case TabControlMessage::CREATE_TAB:
        AddField("subtype", "CREATE_TAB", output);
        break;
      case TabControlMessage::CLOSE_TAB:
        AddField("subtype", "CLOSE_TAB", output);
        break;
      case TabControlMessage::SIZE:
        AddField("subtype", "SIZE", output);
        AddField("size", message.tab_control().size(), output);
        break;
      default:  // unknown
        break;
    }
  }
};

// No fields are extracted from |message|.
class NullLogExtractor : public LogExtractor {
  void ExtractFields(const BlimpMessage& message,
                     LogFields* output) const override {}
};

}  // namespace

BlimpMessageLogger::BlimpMessageLogger() {
  AddHandler("COMPOSITOR", BlimpMessage::COMPOSITOR,
             base::WrapUnique(new CompositorLogExtractor));
  AddHandler("INPUT", BlimpMessage::INPUT,
             base::WrapUnique(new InputLogExtractor));
  AddHandler("NAVIGATION", BlimpMessage::NAVIGATION,
             base::WrapUnique(new NavigationLogExtractor));
  AddHandler("PROTOCOL_CONTROL", BlimpMessage::PROTOCOL_CONTROL,
             base::WrapUnique(new ProtocolControlLogExtractor));
  AddHandler("RENDER_WIDGET", BlimpMessage::RENDER_WIDGET,
             base::WrapUnique(new RenderWidgetLogExtractor));
  AddHandler("SETTINGS", BlimpMessage::SETTINGS,
             base::WrapUnique(new SettingsLogExtractor));
  AddHandler("TAB_CONTROL", BlimpMessage::TAB_CONTROL,
             base::WrapUnique(new TabControlLogExtractor));
}

BlimpMessageLogger::~BlimpMessageLogger() {}

void BlimpMessageLogger::AddHandler(const std::string& type_name,
                                    BlimpMessage::Type type,
                                    std::unique_ptr<LogExtractor> extractor) {
  DCHECK(extractors_.find(type) == extractors_.end());
  DCHECK(!type_name.empty());
  extractors_[type] = make_pair(type_name, std::move(extractor));
}

void BlimpMessageLogger::LogMessageToStream(const BlimpMessage& message,
                                            std::ostream* out) const {
  LogFields fields;

  auto extractor = extractors_.find(message.type());
  if (extractor != extractors_.end()) {
    // An extractor is registered for |message|.
    // Add the human-readable name of |message.type|.
    fields.push_back(make_pair("type", extractor->second.first));
    extractor->second.second->ExtractFields(message, &fields);
  } else {
    // Don't know the human-readable name of |message.type|.
    // Just represent it using its numeric form instead.
    AddField("type", message.type(), &fields);
  }

  // Append "target_tab_id" (if present) and "byte_size" to the field set.
  if (message.has_target_tab_id()) {
    AddField("target_tab_id", message.target_tab_id(), &fields);
  }
  AddField("byte_size", message.ByteSize(), &fields);

  // Format message using the syntax:
  // <BlimpMessage field1=value1 field2="value 2">
  *out << "<BlimpMessage ";
  for (size_t i = 0; i < fields.size(); ++i) {
    *out << fields[i].first << "=" << fields[i].second
         << (i != fields.size() - 1 ? " " : "");
  }
  *out << ">";
}

std::ostream& operator<<(std::ostream& out, const BlimpMessage& message) {
  g_logger.Get().LogMessageToStream(message, &out);
  return out;
}

}  // namespace blimp
