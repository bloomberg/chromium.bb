// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/external_host_bindings.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

using WebKit::WebBindings;
using webkit_glue::CppArgumentList;
using webkit_glue::CppVariant;

ExternalHostBindings::ExternalHostBindings(IPC::Message::Sender* sender,
                                           int routing_id)
    : frame_(NULL), sender_(sender), routing_id_(routing_id) {
  BindCallback("postMessage", base::Bind(&ExternalHostBindings::PostMessage,
                                         base::Unretained(this)));
  BindProperty("onmessage", &on_message_handler_);
}

ExternalHostBindings::~ExternalHostBindings() {
}

void ExternalHostBindings::PostMessage(
    const CppArgumentList& args, CppVariant* result) {
  DCHECK(result);

  // We need at least one argument (message) and at most 2 arguments.
  // Also, the first argument must be a string
  if (args.size() < 1 || args.size() > 2 || !args[0].isString()) {
    result->Set(false);
    return;
  }

  const std::string& message = args[0].ToString();
  std::string target;
  if (args.size() >= 2 && args[1].isString()) {
    target = args[1].ToString();
    if (target.compare("*") != 0) {
      GURL resolved(target);
      if (!resolved.is_valid()) {
        DLOG(WARNING) << "Unable to parse the specified target URL. " << target;
        result->Set(false);
        return;
      }
      target = resolved.spec();
    }
  } else {
    target = "*";
  }

  std::string origin = frame_->document().securityOrigin().toString().utf8();

  result->Set(sender_->Send(
      new ChromeViewHostMsg_ForwardMessageToExternalHost(
          routing_id_, message, origin, target)));
}

bool ExternalHostBindings::ForwardMessageFromExternalHost(
    const std::string& message, const std::string& origin,
    const std::string& target) {
  if (!on_message_handler_.isObject())
    return false;

  bool status = false;

  if (target.compare("*") != 0) {
    // TODO(abarth): This code should use WebSecurityOrigin::toString to
    // make origin strings. GURL::GetOrigin() doesn't understand all the
    // cases that WebSecurityOrigin::toString understands.
    GURL document_url(frame_->document().url());
    GURL document_origin(document_url.GetOrigin());
    GURL target_origin(GURL(target).GetOrigin());

    // We want to compare the origins of the two URLs but first
    // we need to make sure that we don't compare an invalid one
    // to a valid one.
    bool drop = (document_origin.is_valid() != target_origin.is_valid());

    if (!drop) {
      if (!document_origin.is_valid()) {
        // Both origins are invalid, so compare the URLs as opaque strings.
        drop = (document_url.spec().compare(target) != 0);
      } else {
        drop = (document_origin != target_origin);
      }
    }

    if (drop) {
      DLOG(WARNING) << "Dropping posted message.  Origins don't match";
      return false;
    }
  }

  // Construct an event object, assign the origin to the origin member and
  // assign message parameter to the 'data' member of the event.
  NPObject* event_obj = NULL;
  CreateMessageEvent(&event_obj);
  if (!event_obj) {
    NOTREACHED() << "CreateMessageEvent failed";
  } else {
    NPIdentifier init_message_event =
        WebBindings::getStringIdentifier("initMessageEvent");
    NPVariant init_args[8];
    STRINGN_TO_NPVARIANT("message", sizeof("message") - 1,
                         init_args[0]);  // type
    BOOLEAN_TO_NPVARIANT(false, init_args[1]);  // canBubble
    BOOLEAN_TO_NPVARIANT(true, init_args[2]);  // cancelable
    STRINGN_TO_NPVARIANT(message.c_str(), message.length(), \
                         init_args[3]);  // data
    STRINGN_TO_NPVARIANT(origin.c_str(), origin.length(), \
                         init_args[4]);  // origin
    STRINGN_TO_NPVARIANT("", 0, init_args[5]);  // lastEventId
    NULL_TO_NPVARIANT(init_args[6]);  // source
    NULL_TO_NPVARIANT(init_args[7]);  // messagePort

    NPVariant result;
    NULL_TO_NPVARIANT(result);
    status = WebBindings::invoke(NULL, event_obj, init_message_event, init_args,
                                 arraysize(init_args), &result);
    DCHECK(status) << "Failed to initialize MessageEvent";
    WebBindings::releaseVariantValue(&result);

    if (status) {
      NPVariant event_arg;
      OBJECT_TO_NPVARIANT(event_obj, event_arg);
      status = WebBindings::invokeDefault(NULL,
                                          on_message_handler_.value.objectValue,
                                          &event_arg, 1, &result);
      // Don't DCHECK here in case the reason for the failure is a script error.
      DLOG_IF(ERROR, !status) << "NPN_InvokeDefault failed";
      WebBindings::releaseVariantValue(&result);
    }

    WebBindings::releaseObject(event_obj);
  }

  return status;
}

void ExternalHostBindings::BindToJavascript(WebKit::WebFrame* frame,
                                            const std::string& classname) {
  frame_ = frame;
  CppBoundClass::BindToJavascript(frame, classname);
}

bool ExternalHostBindings::CreateMessageEvent(NPObject** message_event) {
  DCHECK(message_event != NULL);
  DCHECK(frame_ != NULL);

  NPObject* window = frame_->windowObject();
  if (!window) {
    NOTREACHED() << "frame_->windowObject";
    return false;
  }

  const char* identifier_names[] = {
    "document",
    "createEvent",
  };

  NPIdentifier identifiers[arraysize(identifier_names)] = {0};
  WebBindings::getStringIdentifiers(identifier_names,
                                    arraysize(identifier_names), identifiers);

  CppVariant document;
  bool ok = WebBindings::getProperty(NULL, window, identifiers[0], &document);
  DCHECK(document.isObject());

  bool success = false;
  if (ok && document.isObject()) {
    NPVariant result, event_type;
    STRINGN_TO_NPVARIANT("MessageEvent", sizeof("MessageEvent") - 1, \
                         event_type);
    success = WebBindings::invoke(NULL, document.value.objectValue,
                                  identifiers[1], &event_type, 1, &result);
    DCHECK(!success || result.type == NPVariantType_Object);
    if (result.type != NPVariantType_Object) {
      DCHECK(success == false);
    } else {
      DCHECK(success != false);
      // Pass the ownership to the caller (don't call ReleaseVariantValue).
      *message_event = result.value.objectValue;
    }
  }

  return success;
}
