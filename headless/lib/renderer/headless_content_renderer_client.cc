// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/renderer/headless_content_renderer_client.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/renderer/render_frame.h"
#include "printing/features/features.h"

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
#include "components/printing/renderer/print_web_view_helper.h"
#include "headless/lib/renderer/headless_print_web_view_helper_delegate.h"
#endif

namespace headless {

HeadlessContentRendererClient::HeadlessContentRendererClient() {}

HeadlessContentRendererClient::~HeadlessContentRendererClient() {}

void HeadlessContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_BASIC_PRINTING)
  new printing::PrintWebViewHelper(
      render_frame, base::MakeUnique<HeadlessPrintWebViewHelperDelegate>());
#endif
}

void HeadlessContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  if (!(render_frame->GetEnabledBindings() &
        content::BindingsPolicy::BINDINGS_POLICY_HEADLESS)) {
    return;
  }

  render_frame->ExecuteJavaScript(base::UTF8ToUTF16(R"(
      (function() {
      let tabSocket = null;
      let messagesToSend = [];

      function getNextEmbedderMessage() {
        tabSocket.awaitNextMessageFromEmbedder().then(
            function(result) {
              window.TabSocket.onmessage(new CustomEvent(
                  "onmessage", {detail : {message : result.message}}));
              getNextEmbedderMessage();
            });
      };

      window.TabSocket = new Proxy(
        {
          send: function(message) {
            if (tabSocket) {
              tabSocket.sendMessageToEmbedder(message);
            } else {
              messagesToSend.push(message);
            }
          },
          onmessage: null
        },
        {
          set: function(target, prop, value, receiver) {
            if (prop !== "onmessage")
              return false;
            target[prop] = value;
            if (tabSocket)
              getNextEmbedderMessage();
            return true;
          }
        });

      // Note define() defines a module in the mojo module dependency
      // system. While we don't expose our module, the callback below only
      // fires after the requested modules have been loaded.
      define([
          'headless/lib/tab_socket.mojom',
          'content/public/renderer/frame_interfaces',
          ], function(tabSocketMojom, frameInterfaces) {
        tabSocket = new tabSocketMojom.TabSocketPtr(
            frameInterfaces.getInterface(tabSocketMojom.TabSocket.name));
        // Send any messages that may have been created before the dependency
        // was resolved.
        for (var i = 0; i < messagesToSend.length; i++) {
          tabSocket.sendMessageToEmbedder(messagesToSend[i]);
        }
        messagesToSend = [];
        if (window.TabSocket.onmessage)
          getNextEmbedderMessage();
      });

      })(); )"));
}

}  // namespace headless
