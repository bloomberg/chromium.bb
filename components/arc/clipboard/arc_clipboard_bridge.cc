// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/clipboard/arc_clipboard_bridge.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "components/arc/arc_bridge_service.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_types.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace arc {

ArcClipboardBridge::ArcClipboardBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->clipboard()->AddObserver(this);
}

ArcClipboardBridge::~ArcClipboardBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  arc_bridge_service()->clipboard()->RemoveObserver(this);
}

void ArcClipboardBridge::OnInstanceReady() {
  mojom::ClipboardInstance* clipboard_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service()->clipboard(), Init);
  DCHECK(clipboard_instance);
  mojom::ClipboardHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  clipboard_instance->Init(std::move(host_proxy));
}

void ArcClipboardBridge::SetTextContent(const std::string& text) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ui::ScopedClipboardWriter writer(ui::CLIPBOARD_TYPE_COPY_PASTE);
  writer.WriteText(base::UTF8ToUTF16(text));
}

void ArcClipboardBridge::GetTextContent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::string16 text;
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &text);

  mojom::ClipboardInstance* clipboard_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->clipboard(), OnGetTextContent);
  if (!clipboard_instance)
    return;
  clipboard_instance->OnGetTextContent(base::UTF16ToUTF8(text));
}

}  // namespace arc
