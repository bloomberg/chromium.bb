// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FONT_CACHE_DISPATCHER_WIN_H_
#define CONTENT_COMMON_FONT_CACHE_DISPATCHER_WIN_H_
#pragma once

#include <windows.h>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "ipc/ipc_channel_proxy.h"

// Dispatches messages used for font caching on Windows. This is needed because
// Windows can't load fonts into its kernel cache in sandboxed processes. So the
// sandboxed process asks the browser process to do this for it.
class FontCacheDispatcher : public IPC::ChannelProxy::MessageFilter,
                            public IPC::Message::Sender {
 public:
  FontCacheDispatcher();
  virtual ~FontCacheDispatcher();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* message) OVERRIDE;

 private:
  // IPC::ChannelProxy::MessageFilter implementation:
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  // Message handlers.
  void OnPreCacheFont(const LOGFONT& font);
  void OnReleaseCachedFonts();

  IPC::Channel* channel_;

  DISALLOW_COPY_AND_ASSIGN(FontCacheDispatcher);
};

#endif  // CONTENT_COMMON_FONT_CACHE_DISPATCHER_WIN_H_
