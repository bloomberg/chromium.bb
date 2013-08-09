// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount_node_tty.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "nacl_io/ioctl.h"
#include "nacl_io/mount.h"
#include "nacl_io/pepper_interface.h"
#include "sdk_util/auto_lock.h"

#define CHECK_LFLAG(TERMIOS, FLAG) (TERMIOS .c_lflag & FLAG)

#define IS_ECHO CHECK_LFLAG(termios_, ECHO)
#define IS_ECHOE CHECK_LFLAG(termios_, ECHOE)
#define IS_ECHONL CHECK_LFLAG(termios_, ECHONL)
#define IS_ECHOCTL CHECK_LFLAG(termios_, ECHOCTL)
#define IS_ICANON CHECK_LFLAG(termios_, ICANON)

namespace nacl_io {

MountNodeTty::MountNodeTty(Mount* mount) : MountNodeCharDevice(mount),
                                           is_readable_(false) {
  pthread_cond_init(&is_readable_cond_, NULL);
  InitTermios();
}

void MountNodeTty::InitTermios() {
  // Some sane values that produce good result.
  termios_.c_iflag = ICRNL | IXON | IXOFF | IUTF8;
  termios_.c_oflag = OPOST | ONLCR;
  termios_.c_cflag = CREAD | 077;
  termios_.c_lflag =
      ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;
  termios_.c_ispeed = B38400;
  termios_.c_ospeed = B38400;
  termios_.c_cc[VINTR] = 3;
  termios_.c_cc[VQUIT] = 28;
  termios_.c_cc[VERASE] = 127;
  termios_.c_cc[VKILL] = 21;
  termios_.c_cc[VEOF] = 4;
  termios_.c_cc[VTIME] = 0;
  termios_.c_cc[VMIN] = 1;
  termios_.c_cc[VSWTC] = 0;
  termios_.c_cc[VSTART] = 17;
  termios_.c_cc[VSTOP] = 19;
  termios_.c_cc[VSUSP] = 26;
  termios_.c_cc[VEOL] = 0;
  termios_.c_cc[VREPRINT] = 18;
  termios_.c_cc[VDISCARD] = 15;
  termios_.c_cc[VWERASE] = 23;
  termios_.c_cc[VLNEXT] = 22;
  termios_.c_cc[VEOL2] = 0;
}

MountNodeTty::~MountNodeTty() {
  pthread_cond_destroy(&is_readable_cond_);
}

Error MountNodeTty::Write(size_t offs,
                     const void* buf,
                     size_t count,
                     int* out_bytes) {
  return Write(offs, buf, count, out_bytes, false);
}

Error MountNodeTty::Write(size_t offs,
                     const void* buf,
                     size_t count,
                     int* out_bytes,
                     bool locked) {
  *out_bytes = 0;

  if (!mount_->ppapi())
    return ENOSYS;

  MessagingInterface* msg_intr = mount_->ppapi()->GetMessagingInterface();
  VarInterface* var_intr = mount_->ppapi()->GetVarInterface();

  if (!(var_intr && msg_intr))
    return ENOSYS;

  // We append the prefix_ to the data in buf, then package it up
  // and post it as a message.
  const char* data = static_cast<const char*>(buf);
  std::string message;
  if (locked) {
    message = prefix_;
  } else {
    AUTO_LOCK(node_lock_);
    message = prefix_;
  }

  message.append(data, count);
  uint32_t len = static_cast<uint32_t>(message.size());
  struct PP_Var val = var_intr->VarFromUtf8(message.data(), len);
  msg_intr->PostMessage(mount_->ppapi()->GetInstance(), val);
  var_intr->Release(val);
  *out_bytes = count;
  return 0;
}

Error MountNodeTty::Read(size_t offs, void* buf, size_t count, int* out_bytes) {
  AUTO_LOCK(node_lock_);
  while (!is_readable_) {
    pthread_cond_wait(&is_readable_cond_, node_lock_.mutex());
  }

  size_t bytes_to_copy = std::min(count, input_buffer_.size());

  if (IS_ICANON) {
    // Only read up to (and including) the first newline
    std::deque<char>::iterator nl = std::find(input_buffer_.begin(),
                                              input_buffer_.end(),
                                              '\n');

    if (nl != input_buffer_.end()) {
      // We found a newline in the buffer, adjust bytes_to_copy accordingly
      size_t line_len = static_cast<size_t>(nl - input_buffer_.begin()) + 1;
      bytes_to_copy = std::min(bytes_to_copy, line_len);
    }
  }


  // Copies data from the input buffer into buf.
  std::copy(input_buffer_.begin(), input_buffer_.begin() + bytes_to_copy,
            static_cast<char*>(buf));
  *out_bytes = bytes_to_copy;
  input_buffer_.erase(input_buffer_.begin(),
                      input_buffer_.begin() + bytes_to_copy);

  // mark input as no longer readable if we consumed
  // the entire buffer or, in the case of buffered input,
  // we consumed the final \n char.
  if (IS_ICANON)
    is_readable_ =
        std::find(input_buffer_.begin(),
                  input_buffer_.end(), '\n') != input_buffer_.end();
  else
    is_readable_ = input_buffer_.size() > 0;

  return 0;
}

Error MountNodeTty::Echo(const char* string, int count) {
  int wrote;
  Error error = Write(0, string, count, &wrote, true);
  if (error != 0 || wrote != count) {
    // TOOD(sbc): Do something more useful in response to a
    // failure to echo.
    return error;
  }

  return 0;
}

Error MountNodeTty::ProcessInput(struct tioc_nacl_input_string* message) {
  AUTO_LOCK(node_lock_);
  if (message->length < prefix_.size() ||
      strncmp(message->buffer, prefix_.data(), prefix_.size()) != 0) {
    return ENOTTY;
  }

  const char* buffer = message->buffer + prefix_.size();
  int num_bytes = message->length - prefix_.size();

  for (int i = 0; i < num_bytes; i++) {
    char c = buffer[i];
    // Transform characters according to input flags.
    if (c == '\r') {
      if (termios_.c_iflag & IGNCR)
        continue;
      if (termios_.c_iflag & ICRNL)
        c = '\n';
    } else if (c == '\n') {
      if (termios_.c_iflag & INLCR)
        c = '\r';
    }

    bool skip = false;

    // ICANON mode means we wait for a newline before making the
    // file readable.
    if (IS_ICANON) {
      if (IS_ECHOE && c == termios_.c_cc[VERASE]) {
        // Remove previous character in the line if any.
        if (!input_buffer_.empty()) {
          char char_to_delete = input_buffer_.back();
          if (char_to_delete != '\n') {
            input_buffer_.pop_back();
            if (IS_ECHO)
              Echo("\b \b", 3);

            // When ECHOCTL is set the echo buffer contains an extra
            // char for each control char.
            if (IS_ECHOCTL && iscntrl(char_to_delete))
              Echo("\b \b", 3);
          }
        }
        continue;
      } else if (IS_ECHO || (IS_ECHONL && c == '\n')) {
        if (c == termios_.c_cc[VEOF]) {
          // VEOF sequence is not echoed, nor is it sent as
          // input.
          skip = true;
        } else if (c != '\n' && iscntrl(c) && IS_ECHOCTL) {
          // In ECHOCTL mode a control char C is echoed  as '^'
          // followed by the ascii char which at C + 0x40.
          char visible_char = c + 0x40;
          Echo("^", 1);
          Echo(&visible_char, 1);
        } else {
          Echo(&c, 1);
        }
      }
    }

    if (!skip)
      input_buffer_.push_back(c);

    if (c == '\n' || c == termios_.c_cc[VEOF] || !IS_ICANON)
      is_readable_ = true;
  }

  if (is_readable_)
    pthread_cond_broadcast(&is_readable_cond_);
  return 0;
}

Error MountNodeTty::Ioctl(int request, char* arg) {
  if (request == TIOCNACLPREFIX) {
    // This ioctl is used to change the prefix for this tty node.
    // The prefix is used to distinguish messages intended for this
    // tty node from all the other messages cluttering up the
    // javascript postMessage() channel.
    AUTO_LOCK(node_lock_);
    prefix_ = arg;
    return 0;
  } else if (request == TIOCNACLINPUT) {
    // This ioctl is used to deliver data from the user to this tty node's
    // input buffer. We check if the prefix in the input data matches the
    // prefix for this node, and only deliver the data if so.
    struct tioc_nacl_input_string* message =
      reinterpret_cast<struct tioc_nacl_input_string*>(arg);
    return ProcessInput(message);
  } else {
    return EINVAL;
  }
}

Error MountNodeTty::Tcgetattr(struct termios* termios_p) {
  AUTO_LOCK(node_lock_);
  *termios_p = termios_;
  return 0;
}

Error MountNodeTty::Tcsetattr(int optional_actions,
                         const struct termios *termios_p) {
  AUTO_LOCK(node_lock_);
  termios_ = *termios_p;
  return 0;
}

}
