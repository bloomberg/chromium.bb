// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount_node_tty.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

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

#define DEFAULT_TTY_COLS 80
#define DEFAULT_TTY_ROWS 30

namespace nacl_io {

MountNodeTty::MountNodeTty(Mount* mount) : MountNodeCharDevice(mount),
                                           is_readable_(false),
                                           did_resize_(false),
                                           rows_(DEFAULT_TTY_ROWS),
                                           cols_(DEFAULT_TTY_COLS) {
  output_handler_.handler = NULL;
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
  AUTO_LOCK(output_lock_);
  *out_bytes = 0;

  // No handler registered.
  if (output_handler_.handler == NULL)
    return EIO;

  int rtn = output_handler_.handler(static_cast<const char*>(buf),
                                    count,
                                    output_handler_.user_data);

  // Negative return value means an error occured and the return
  // value is a negated errno value.
  if (rtn < 0)
    return -rtn;

  *out_bytes = rtn;
  return 0;
}

Error MountNodeTty::Read(size_t offs, void* buf, size_t count, int* out_bytes) {
  AUTO_LOCK(node_lock_);
  did_resize_ = false;
  while (!is_readable_) {
    pthread_cond_wait(&is_readable_cond_, node_lock_.mutex());
    if (!is_readable_ && did_resize_) {
      // If an async resize event occured then return the failure and
      // set EINTR.
      *out_bytes = 0;
      return EINTR;
    }
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
  Error error = Write(0, string, count, &wrote);
  if (error != 0 || wrote != count) {
    // TOOD(sbc): Do something more useful in response to a
    // failure to echo.
    return error;
  }

  return 0;
}

Error MountNodeTty::ProcessInput(struct tioc_nacl_input_string* message) {
  AUTO_LOCK(node_lock_);

  const char* buffer = message->buffer;
  size_t num_bytes = message->length;

  for (size_t i = 0; i < num_bytes; i++) {
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

  if (is_readable_) {
    RaiseEvent(POLLIN);
    pthread_cond_broadcast(&is_readable_cond_);
  }

  return 0;
}

Error MountNodeTty::Ioctl(int request, char* arg) {
  switch (request) {
    case TIOCNACLOUTPUT: {
      AUTO_LOCK(output_lock_);
      if (arg == NULL) {
        output_handler_.handler = NULL;
        return 0;
      }
      if (output_handler_.handler != NULL)
        return EALREADY;
      output_handler_ = *reinterpret_cast<tioc_nacl_output*>(arg);
      return 0;
    }
    case TIOCNACLINPUT: {
      // This ioctl is used to deliver data from the user to this tty node's
      // input buffer.
      struct tioc_nacl_input_string* message =
        reinterpret_cast<struct tioc_nacl_input_string*>(arg);
      return ProcessInput(message);
    }
    case TIOCSWINSZ: {
      struct winsize* size = reinterpret_cast<struct winsize*>(arg);
      {
        AUTO_LOCK(node_lock_);
        rows_ = size->ws_row;
        cols_ = size->ws_col;
      }
      kill(getpid(), SIGWINCH);

      // Wake up any thread waiting on Read
      {
        AUTO_LOCK(node_lock_);
        did_resize_ = true;
        pthread_cond_broadcast(&is_readable_cond_);
      }
      return 0;
    }
    case TIOCGWINSZ: {
      struct winsize* size = reinterpret_cast<struct winsize*>(arg);
      size->ws_row = rows_;
      size->ws_col = cols_;
      return 0;
    }
  }

  return EINVAL;
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
