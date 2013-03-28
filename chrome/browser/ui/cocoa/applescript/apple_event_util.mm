// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/apple_event_util.h"

#import <Carbon/Carbon.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"

namespace chrome {
namespace mac {

NSAppleEventDescriptor* ValueToAppleEventDescriptor(const base::Value* value) {
  NSAppleEventDescriptor* descriptor = nil;

  switch (value->GetType()) {
    case base::Value::TYPE_NULL:
      descriptor = [NSAppleEventDescriptor
          descriptorWithTypeCode:cMissingValue];
      break;

    case base::Value::TYPE_BOOLEAN: {
      bool bool_value;
      value->GetAsBoolean(&bool_value);
      descriptor = [NSAppleEventDescriptor descriptorWithBoolean:bool_value];
      break;
    }

    case base::Value::TYPE_INTEGER: {
      int int_value;
      value->GetAsInteger(&int_value);
      descriptor = [NSAppleEventDescriptor descriptorWithInt32:int_value];
      break;
    }

    case base::Value::TYPE_DOUBLE: {
      double double_value;
      value->GetAsDouble(&double_value);
      descriptor = [NSAppleEventDescriptor
          descriptorWithDescriptorType:typeIEEE64BitFloatingPoint
                                 bytes:&double_value
                                length:sizeof(double_value)];
      break;
    }

    case base::Value::TYPE_STRING: {
      std::string string_value;
      value->GetAsString(&string_value);
      descriptor = [NSAppleEventDescriptor descriptorWithString:
          base::SysUTF8ToNSString(string_value)];
      break;
    }

    case base::Value::TYPE_BINARY:
      NOTREACHED();
      break;

    case base::Value::TYPE_DICTIONARY: {
      const base::DictionaryValue* dictionary_value;
      value->GetAsDictionary(&dictionary_value);
      descriptor = [NSAppleEventDescriptor recordDescriptor];
      NSAppleEventDescriptor* userRecord = [NSAppleEventDescriptor
          listDescriptor];
      for (DictionaryValue::Iterator iter(*dictionary_value); !iter.IsAtEnd();
           iter.Advance()) {
        [userRecord insertDescriptor:[NSAppleEventDescriptor
            descriptorWithString:base::SysUTF8ToNSString(iter.key())]
                             atIndex:0];
        [userRecord insertDescriptor:ValueToAppleEventDescriptor(&iter.value())
                             atIndex:0];
      }
      // Description of what keyASUserRecordFields does.
      // http://lists.apple.com/archives/cocoa-dev/2009/Jul/msg01216.html
      [descriptor setDescriptor:userRecord forKeyword:keyASUserRecordFields];
      break;
    }

    case base::Value::TYPE_LIST: {
      const base::ListValue* list_value;
      value->GetAsList(&list_value);
      descriptor = [NSAppleEventDescriptor listDescriptor];
      for (size_t i = 0; i < list_value->GetSize(); ++i) {
        const base::Value* item;
        list_value->Get(i, &item);
        [descriptor insertDescriptor:ValueToAppleEventDescriptor(item)
                             atIndex:0];
      }
      break;
    }
  }

  return descriptor;
}

}  // namespace mac
}  // namespace chrome
