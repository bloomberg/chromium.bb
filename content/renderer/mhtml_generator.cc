// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mhtml_generator.h"

#include "base/platform_file.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageSerializer.h"

MHTMLGenerator::MHTMLGenerator(RenderViewImpl* render_view)
    : content::RenderViewObserver(render_view),
      file_(base::kInvalidPlatformFileValue) {
}

MHTMLGenerator::~MHTMLGenerator() {
}

// RenderViewObserver implementation:
bool MHTMLGenerator::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MHTMLGenerator, message)
      IPC_MESSAGE_HANDLER(ViewMsg_SavePageAsMHTML, OnSavePageAsMHTML)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MHTMLGenerator::OnSavePageAsMHTML(
    int job_id, IPC::PlatformFileForTransit file_for_transit) {
  base::PlatformFile file =
      IPC::PlatformFileForTransitToPlatformFile(file_for_transit);
  file_ = file;
  int64 size = GenerateMHTML();
  base::ClosePlatformFile(file);
  NotifyBrowser(job_id, size);
}

void MHTMLGenerator::NotifyBrowser(int job_id, int64 data_size) {
  render_view()->Send(new ViewHostMsg_SavedPageAsMHTML(job_id, data_size));
  file_ = base::kInvalidPlatformFileValue;
}

// TODO(jcivelli): write the chunks in deferred tasks to give a chance to the
//                 message loop to process other events.
int64 MHTMLGenerator::GenerateMHTML() {
  WebKit::WebCString mhtml =
      WebKit::WebPageSerializer::serializeToMHTML(render_view()->GetWebView());
  const size_t chunk_size = 1024;
  const char* data = mhtml.data();
  size_t total_bytes_written = 0;
  while (total_bytes_written < mhtml.length()) {
    size_t copy_size =
        std::min(mhtml.length() - total_bytes_written, chunk_size);
    int bytes_written = base::WritePlatformFile(file_, total_bytes_written,
                                                data + total_bytes_written,
                                                copy_size);
    if (bytes_written == -1)
      return -1;
    total_bytes_written += bytes_written;
  }
  return total_bytes_written;
}
