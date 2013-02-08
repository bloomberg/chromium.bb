// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_INTENT_SERVICE_HOST_H_
#define CHROME_BROWSER_INTENTS_INTENT_SERVICE_HOST_H_

namespace content {
class WebIntentsDispatcher;
}

namespace web_intents {

// Interface for web intent "services" that act as handlers for
// web intents invoked by client code or other means. "services" can
// be implemented in a variety of means across different "host" environments
// Examples:
// * In another tab, where the tab hosts a web page that services the
//   intent.
// * In the Chrome browser where a file picker can be used to select
//   a file from disk in response to a "pick" action.
// * In the underlying OS where a native application could be used
//   to handle an intent.
//
// A "service" is selected by policy, based on a heuristic including:
// user selection, system defaults, declaration as an "explicit" intent
// by client code, and other means. This selection process is managed
// by WebInentPickerController.
//
// A service, once selected, is responsible for responding to an intent.
// The "intent" is presented to the service as an instance of
// WebIntentsDispatcher. The dispatcher provides access to the intent data
// in the form of a webkit_glue::WebIntentData. The dispatcher also provides
// the service a means of responding to the intent.
//
// A service instance is owned by WebIntentPickerController. Its lifetime is a
// subset of the life of an intent. It will be created immediately prior
// to calling HandleIntent, and destroyed immediately after SendReply
// is called.
//
// For details see content::WebIntentsDispatcher and webkit_glue::WebIntentData
class IntentServiceHost {
 public:
  virtual ~IntentServiceHost() {}

  // A service is responsible for replying to the dispatcher in all
  // circumstances, including when the user has canceled the operation,
  // or the action was terminated without user intervention. In each of
  // these cases the correct webkit_glue::WebIntentReplyType should
  // be chosen and a response should be delivered to the dispatcher
  // via SendReply(webkit_glue::WebIntentReply).
  virtual void HandleIntent(content::WebIntentsDispatcher* dispatcher) = 0;
};

}  // namespace web_intents

#endif  // CHROME_BROWSER_INTENTS_INTENT_SERVICE_HOST_H_
