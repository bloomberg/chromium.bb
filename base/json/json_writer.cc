// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"

#include <cmath>

#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace base {

#if defined(OS_WIN)
const char kPrettyPrintLineEnding[] = "\r\n";
#else
const char kPrettyPrintLineEnding[] = "\n";
#endif

// static
void JSONWriter::Write(const Value* const node, std::string* json) {
  WriteWithOptions(node, 0, json);
}

// static
void JSONWriter::WriteWithOptions(const Value* const node, int options,
                                  std::string* json) {
  json->clear();
  // Is there a better way to estimate the size of the output?
  json->reserve(1024);

  JSONWriter writer(options, json);
  writer.BuildJSONString(node, 0U);

  if (options & OPTIONS_PRETTY_PRINT)
    json->append(kPrettyPrintLineEnding);
}

JSONWriter::JSONWriter(int options, std::string* json)
    : omit_binary_values_((options & OPTIONS_OMIT_BINARY_VALUES) != 0),
      omit_double_type_preservation_(
          (options & OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION) != 0),
      pretty_print_((options & OPTIONS_PRETTY_PRINT) != 0),
      json_string_(json) {
  DCHECK(json);
}

void JSONWriter::BuildJSONString(const Value* const node, size_t depth) {
  switch (node->GetType()) {
    case Value::TYPE_NULL: {
      json_string_->append("null");
      break;
    }

    case Value::TYPE_BOOLEAN: {
      bool value;
      bool result = node->GetAsBoolean(&value);
      DCHECK(result);
      json_string_->append(value ? "true" : "false");
      break;
    }

    case Value::TYPE_INTEGER: {
      int value;
      bool result = node->GetAsInteger(&value);
      DCHECK(result);
      json_string_->append(IntToString(value));
      break;
    }

    case Value::TYPE_DOUBLE: {
      double value;
      bool result = node->GetAsDouble(&value);
      DCHECK(result);
      if (omit_double_type_preservation_ &&
          value <= kint64max &&
          value >= kint64min &&
          std::floor(value) == value) {
        json_string_->append(Int64ToString(static_cast<int64>(value)));
        break;
      }
      std::string real = DoubleToString(value);
      // Ensure that the number has a .0 if there's no decimal or 'e'.  This
      // makes sure that when we read the JSON back, it's interpreted as a
      // real rather than an int.
      if (real.find('.') == std::string::npos &&
          real.find('e') == std::string::npos &&
          real.find('E') == std::string::npos) {
        real.append(".0");
      }
      // The JSON spec requires that non-integer values in the range (-1,1)
      // have a zero before the decimal point - ".52" is not valid, "0.52" is.
      if (real[0] == '.') {
        real.insert(0U, 1U, '0');
      } else if (real.length() > 1 && real[0] == '-' && real[1] == '.') {
        // "-.1" bad "-0.1" good
        real.insert(1U, 1U, '0');
      }
      json_string_->append(real);
      break;
    }

    case Value::TYPE_STRING: {
      std::string value;
      bool result = node->GetAsString(&value);
      DCHECK(result);
      EscapeJSONString(value, true, json_string_);
      break;
    }

    case Value::TYPE_LIST: {
      json_string_->push_back('[');
      if (pretty_print_)
        json_string_->push_back(' ');

      const ListValue* list = static_cast<const ListValue*>(node);
      for (ListValue::const_iterator it = list->begin(); it != list->end();
           ++it) {
        const Value* value = *it;

        if (omit_binary_values_ && value->GetType() == Value::TYPE_BINARY)
          continue;

        if (it != list->begin()) {
          json_string_->push_back(',');
          if (pretty_print_)
            json_string_->push_back(' ');
        }

        BuildJSONString(value, depth);
      }

      if (pretty_print_)
        json_string_->push_back(' ');
      json_string_->push_back(']');
      break;
    }

    case Value::TYPE_DICTIONARY: {
      json_string_->push_back('{');
      if (pretty_print_)
        json_string_->append(kPrettyPrintLineEnding);

      const DictionaryValue* dict =
          static_cast<const DictionaryValue*>(node);
      bool first_entry = true;
      for (DictionaryValue::Iterator itr(*dict); !itr.IsAtEnd();
           itr.Advance(), first_entry = false) {
        if (omit_binary_values_ &&
            itr.value().GetType() == Value::TYPE_BINARY) {
          continue;
        }

        if (!first_entry) {
          json_string_->push_back(',');
          if (pretty_print_)
            json_string_->append(kPrettyPrintLineEnding);
        }

        if (pretty_print_)
          IndentLine(depth + 1U);

        EscapeJSONString(itr.key(), true, json_string_);

        json_string_->push_back(':');
        if (pretty_print_)
          json_string_->push_back(' ');
        BuildJSONString(&itr.value(), depth + 1U);
      }

      if (pretty_print_) {
        json_string_->append(kPrettyPrintLineEnding);
        IndentLine(depth);
        json_string_->push_back('}');
      } else {
        json_string_->push_back('}');
      }
      break;
    }

    case Value::TYPE_BINARY: {
      if (!omit_binary_values_) {
        NOTREACHED() << "Cannot serialize binary value.";
      }
      break;
    }

    default: {
      NOTREACHED() << "unknown json type";
    }
  }
}

void JSONWriter::IndentLine(size_t depth) {
  json_string_->append(depth * 3U, ' ');
}

}  // namespace base
