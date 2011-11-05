// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_tracking.h"

#include <ctype.h>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/tracked_objects.h"
#include "chrome/common/render_messages.h"
#include "content/public/renderer/render_thread.h"

using content::RenderThread;

RendererTracking::RendererTracking()
    : ALLOW_THIS_IN_INITIALIZER_LIST(renderer_tracking_factory_(this)) {
}

RendererTracking::~RendererTracking() {
}

bool RendererTracking::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererTracking, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_GetRendererTrackedData,
                        OnGetRendererTrackingData)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetTrackingStatus,
                        OnSetTrackingStatus)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererTracking::OnGetRendererTrackingData(int sequence_number) {
  RenderThread::Get()->GetMessageLoop()->PostTask(FROM_HERE,
      renderer_tracking_factory_.NewRunnableMethod(
          &RendererTracking::UploadAllTrackingData,
          sequence_number));
}

void RendererTracking::OnSetTrackingStatus(bool enable) {
  tracked_objects::ThreadData::InitializeAndSetTrackingStatus(enable);
}

void RendererTracking::UploadAllTrackingData(int sequence_number) {
  scoped_ptr<base::DictionaryValue> value(
      tracked_objects::ThreadData::ToValue());
  value->SetInteger("process_id", base::GetCurrentProcId());

  std::string pickled_tracking_data;
  base::JSONWriter::Write(value.get(), false, &pickled_tracking_data);

  // Send the sequence number and list of pickled tracking data over synchronous
  // IPC, so we can clear pickled_tracking_data_ afterwards.
  RenderThread::Get()->Send(new ChromeViewHostMsg_RendererTrackedData(
      sequence_number, pickled_tracking_data));
}
