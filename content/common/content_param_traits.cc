// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_param_traits.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "content/common/frame_message_structs.h"
#include "ipc/ipc_mojo_message_helper.h"
#include "ipc/ipc_mojo_param_traits.h"
#include "net/base/ip_endpoint.h"
#include "third_party/WebKit/public/common/message_port/message_port_channel.h"
#include "third_party/WebKit/public/common/message_port/transferable_message.h"
#include "third_party/WebKit/public/mojom/message_port/message_port.mojom.h"
#include "ui/accessibility/ax_modes.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/blink/web_input_event_traits.h"

namespace IPC {

void ParamTraits<WebInputEventPointer>::Write(base::Pickle* m,
                                              const param_type& p) {
  m->WriteData(reinterpret_cast<const char*>(p), p->size());
}

bool ParamTraits<WebInputEventPointer>::Read(const base::Pickle* m,
                                             base::PickleIterator* iter,
                                             param_type* r) {
  const char* data;
  int data_length;
  if (!iter->ReadData(&data, &data_length)) {
    NOTREACHED();
    return false;
  }
  if (data_length < static_cast<int>(sizeof(blink::WebInputEvent))) {
    NOTREACHED();
    return false;
  }
  param_type event = reinterpret_cast<param_type>(data);
  // Check that the data size matches that of the event.
  if (data_length != static_cast<int>(event->size())) {
    NOTREACHED();
    return false;
  }
  const size_t expected_size_for_type =
      ui::WebInputEventTraits::GetSize(event->GetType());
  if (data_length != static_cast<int>(expected_size_for_type)) {
    NOTREACHED();
    return false;
  }
  *r = event;
  return true;
}

void ParamTraits<WebInputEventPointer>::Log(const param_type& p,
                                            std::string* l) {
  l->append("(");
  LogParam(p->size(), l);
  l->append(", ");
  LogParam(p->GetType(), l);
  l->append(", ");
  LogParam(p->TimeStampSeconds(), l);
  l->append(")");
}

void ParamTraits<blink::MessagePortChannel>::Write(base::Pickle* m,
                                                   const param_type& p) {
  ParamTraits<mojo::MessagePipeHandle>::Write(m, p.ReleaseHandle().release());
}

bool ParamTraits<blink::MessagePortChannel>::Read(const base::Pickle* m,
                                                  base::PickleIterator* iter,
                                                  param_type* r) {
  mojo::MessagePipeHandle handle;
  if (!ParamTraits<mojo::MessagePipeHandle>::Read(m, iter, &handle))
    return false;
  *r = blink::MessagePortChannel(mojo::ScopedMessagePipeHandle(handle));
  return true;
}

void ParamTraits<blink::MessagePortChannel>::Log(const param_type& p,
                                                 std::string* l) {}

void ParamTraits<ui::AXMode>::Write(base::Pickle* m, const param_type& p) {
  IPC::WriteParam(m, p.mode());
}

bool ParamTraits<ui::AXMode>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   param_type* r) {
  uint32_t value;
  if (!IPC::ReadParam(m, iter, &value))
    return false;
  *r = ui::AXMode(value);
  return true;
}

void ParamTraits<ui::AXMode>::Log(const param_type& p, std::string* l) {}

void ParamTraits<scoped_refptr<storage::BlobHandle>>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p != nullptr);
  if (p) {
    auto info = p->Clone().PassInterface();
    m->WriteUInt32(info.version());
    MojoMessageHelper::WriteMessagePipeTo(m, info.PassHandle());
  }
}

bool ParamTraits<scoped_refptr<storage::BlobHandle>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool is_not_null;
  if (!ReadParam(m, iter, &is_not_null))
    return false;
  if (!is_not_null)
    return true;

  uint32_t version;
  if (!ReadParam(m, iter, &version))
    return false;
  mojo::ScopedMessagePipeHandle handle;
  if (!MojoMessageHelper::ReadMessagePipeFrom(m, iter, &handle))
    return false;
  DCHECK(handle.is_valid());
  blink::mojom::BlobPtr blob;
  blob.Bind(blink::mojom::BlobPtrInfo(std::move(handle), version));
  *r = base::MakeRefCounted<storage::BlobHandle>(std::move(blob));
  return true;
}

void ParamTraits<scoped_refptr<storage::BlobHandle>>::Log(const param_type& p,
                                                          std::string* l) {
  l->append("<storage::BlobHandle>");
}

// static
void ParamTraits<content::FrameMsg_ViewChanged_Params>::Write(
    base::Pickle* m,
    const param_type& p) {
  DCHECK(base::FeatureList::IsEnabled(features::kMash) ||
         (p.frame_sink_id.has_value() && p.frame_sink_id->is_valid()));
  WriteParam(m, p.frame_sink_id);
}

bool ParamTraits<content::FrameMsg_ViewChanged_Params>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  if (!ReadParam(m, iter, &(r->frame_sink_id)))
    return false;
  if (!base::FeatureList::IsEnabled(features::kMash) &&
      (!r->frame_sink_id || !r->frame_sink_id->is_valid())) {
    NOTREACHED();
    return false;
  }
  return true;
}

// static
void ParamTraits<content::FrameMsg_ViewChanged_Params>::Log(const param_type& p,
                                                            std::string* l) {
  l->append("(");
  LogParam(p.frame_sink_id, l);
  l->append(")");
}

template <>
struct ParamTraits<blink::mojom::SerializedBlobPtr> {
  using param_type = blink::mojom::SerializedBlobPtr;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p->uuid);
    WriteParam(m, p->content_type);
    WriteParam(m, p->size);
    WriteParam(m, p->blob.PassHandle().release());
  }

  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    *r = blink::mojom::SerializedBlob::New();
    mojo::MessagePipeHandle handle;
    if (!ReadParam(m, iter, &(*r)->uuid) ||
        !ReadParam(m, iter, &(*r)->content_type) ||
        !ReadParam(m, iter, &(*r)->size) || !ReadParam(m, iter, &handle)) {
      return false;
    }
    (*r)->blob = blink::mojom::BlobPtrInfo(
        mojo::ScopedMessagePipeHandle(handle), blink::mojom::Blob::Version_);
    return true;
  }
};

void ParamTraits<scoped_refptr<base::RefCountedData<
    blink::TransferableMessage>>>::Write(base::Pickle* m, const param_type& p) {
  m->WriteData(reinterpret_cast<const char*>(p->data.encoded_message.data()),
               p->data.encoded_message.size());
  WriteParam(m, p->data.blobs);
  WriteParam(m, p->data.stack_trace_id);
  WriteParam(m, p->data.stack_trace_debugger_id_first);
  WriteParam(m, p->data.stack_trace_debugger_id_second);
  WriteParam(m, p->data.ports);
  WriteParam(m, p->data.has_user_gesture);
}

bool ParamTraits<
    scoped_refptr<base::RefCountedData<blink::TransferableMessage>>>::
    Read(const base::Pickle* m, base::PickleIterator* iter, param_type* r) {
  *r = new base::RefCountedData<blink::TransferableMessage>();

  const char* data;
  int length;
  if (!iter->ReadData(&data, &length))
    return false;
  // This just makes encoded_message point into the IPC message buffer. Usually
  // code receiving a TransferableMessage will synchronously process the message
  // so this avoids an unnecessary copy. If a receiver needs to hold on to the
  // message longer, it should make sure to call EnsureDataIsOwned on the
  // returned message.
  (*r)->data.encoded_message =
      base::make_span(reinterpret_cast<const uint8_t*>(data), length);
  if (!ReadParam(m, iter, &(*r)->data.blobs) ||
      !ReadParam(m, iter, &(*r)->data.stack_trace_id) ||
      !ReadParam(m, iter, &(*r)->data.stack_trace_debugger_id_first) ||
      !ReadParam(m, iter, &(*r)->data.stack_trace_debugger_id_second) ||
      !ReadParam(m, iter, &(*r)->data.ports) ||
      !ReadParam(m, iter, &(*r)->data.has_user_gesture)) {
    return false;
  }
  return true;
}

void ParamTraits<scoped_refptr<
    base::RefCountedData<blink::TransferableMessage>>>::Log(const param_type& p,
                                                            std::string* l) {
  l->append("<blink::TransferableMessage>");
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#include "content/common/content_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#include "content/common/content_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#include "content/common/content_param_traits_macros.h"
}  // namespace IPC
