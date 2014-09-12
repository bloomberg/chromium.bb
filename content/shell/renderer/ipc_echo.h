// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_IPC_ECHO_H_
#define CONTENT_SHELL_IPC_ECHO_H_

#include <utility>
#include <vector>
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/web/WebDocument.h"

namespace blink {
class WebFrame;
}

namespace IPC {
class Sender;
}

namespace content {

// This class is for writing micro benchmarks exercising underlying
// IPC::Channel using HTML and JavaScript.
//
// TODO(morrita): The benchmark effort tries to makesure that
// IPC::ChannelMojo is as performant as native IPC::Channel
// implementations. Currently IPC::ChannelMojo is hidden behind
// "--enable-renderer-mojo-channel". Once it is turned on by default.
// we no longer need this class and should remove it.
class IPCEcho : public base::SupportsWeakPtr<IPCEcho> {
 public:
  IPCEcho(blink::WebDocument document, IPC::Sender* sender, int routing_id);
  ~IPCEcho();

  void RequestEcho(int id, int size);
  void DidRespondEcho(int id, int size);
  void Install(blink::WebFrame*);

  int last_echo_id() const { return last_echo_id_; }
  int last_echo_size() const { return last_echo_size_; }

  blink::WebDocument document_;
  IPC::Sender* sender_;
  int routing_id_;
  int last_echo_id_;
  int last_echo_size_;

  base::WeakPtrFactory<IPCEcho> weak_factory_;

};

}  // namespace content

#endif  // CONTENT_SHELL_IPC_ECHO_DISPATCHER_H_
