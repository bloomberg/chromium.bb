// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <gflags/gflags.h>
#include <X11/extensions/XI2.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XIproto.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifndef EVIOCSCLOCKID
#define EVIOCSCLOCKID  _IOW('E', 0xa0, int)
#endif

using std::string;
using std::vector;

DEFINE_string(display, "", "X display. Default: use env var DISPLAY");
DEFINE_string(exclude_keycodes, "1,125,42,29,56,100,97,54",
              "Comma separated list of keycodes to ignore. Default excludes "
              "ESC, modifier keys, and search key on US keyboards.");
DEFINE_bool(foreground, false, "Don't daemon()ize; run in foreground.");
DEFINE_string(keyboard_name, "AT Translated Set 2 keyboard",
              "Name of the keyboard to monitor");
DEFINE_string(tp_prop_name_high, "Keyboard Touched Timeval High",
              "Touchpad property to set (timeval high)");
DEFINE_string(tp_prop_name_low, "Keyboard Touched Timeval Low",
              "Touchpad property to set (timeval low)");

namespace keyboard_touchpad_helper {

template<typename Key>
bool VectorContainsKey(const vector<Key>& vect, const Key& key) {
  return std::find(vect.begin(), vect.end(), key) != vect.end();
}

void PerrorAbort(const char* err) {
  perror(err);
  exit(1);
}

vector<unsigned short> ParseExclusions() {
  vector<unsigned short> ret;
  vector<string> strings;
  base::SplitString(FLAGS_exclude_keycodes, ',', &strings);
  for (vector<string>::iterator it = strings.begin(), e = strings.end();
       it != e; ++it)
    ret.push_back(strtol((*it).c_str(), NULL, 10));
  return ret;
}

class XNotifier {
 public:
  XNotifier(Display* display, int tp_id) : display_(display), dev_(NULL) {
    if (!display_) {
      fprintf(stderr, "Unable to connect to X server.\n");
      exit(1);
    }
    dev_ = XOpenDevice(display_, tp_id);
    prop_high_ = XInternAtom(display_, FLAGS_tp_prop_name_high.c_str(), True);
    prop_low_ = XInternAtom(display_, FLAGS_tp_prop_name_low.c_str(), True);
  }
  ~XNotifier() {
    if (dev_) {
      XCloseDevice(display_, dev_);
      dev_ = NULL;
    }
  }
  bool Notify(const struct timeval& tv) {
    if (!dev_)
      return false;
    int high = tv.tv_sec;
    int low = tv.tv_usec;

    XChangeDeviceProperty(display_, dev_, prop_high_, XA_INTEGER, 32,
                          PropModeReplace,
                          reinterpret_cast<const unsigned char*>(&high), 1);
    XChangeDeviceProperty(display_, dev_, prop_low_, XA_INTEGER, 32,
                          PropModeReplace,
                          reinterpret_cast<const unsigned char*>(&low), 1);
    XSync(display_, False);
    return true;
  }

 private:
  Display *display_;
  XDevice* dev_;
  Atom prop_high_;
  Atom prop_low_;
};

string GetKeyboardPath(Display *display) {
  Atom prop = XInternAtom(display, "Device Node", True);
  // Fail quick if server does not have Atom
  if (prop == None)
    return "";

  int num_devices = 0;
  XIDeviceInfo* info = XIQueryDevice(display, XIAllDevices, &num_devices);
  if (!info)
    PerrorAbort("XIQueryDevice");
  int found_devid = -1;
  for (int i = 0; i < num_devices; i++) {
    XIDeviceInfo* device = &info[i];
    if (device->use != XISlaveKeyboard)
      continue;
    if (strcmp(device->name, FLAGS_keyboard_name.c_str()) == 0) {
      found_devid = device->deviceid;
      break;
    }
  }
  XIFreeDeviceInfo(info);
  if (found_devid < 0)
    return "";

  Atom actual_type;
  int actual_format;
  unsigned long num_items, bytes_after;
  unsigned char* data;
  if (XIGetProperty(display, found_devid, prop, 0, 1000, False,
                    XA_STRING, &actual_type, &actual_format,
                    &num_items, &bytes_after, &data) != Success) {
    // XIGetProperty can generate BadAtom, BadValue, and BadWindow errors
    fprintf(stderr, "Mysterious X server error\n");
    return "";
  }
  if (actual_type == None) {
    const char kErr[] =
        "Property didn't exist (not xf86-input-evdev, or -evdev too old?)?\n";
    fprintf(stderr, "%s", kErr);
    return "";
  } else if (actual_type != XA_STRING || actual_format != 8) {
    fprintf(stderr, "Property not string or bad format.\n");
    XFree(data);
    return "";
  }
  string ret = string(reinterpret_cast<char*>(data));
  XFree(data);
  return ret;
}

bool IsCmtTouchpad(Display* display, int device_id) {
  Atom prop = XInternAtom(display, FLAGS_tp_prop_name_low.c_str(), True);
  // Fail quick if server does not have Atom
  if (prop == None)
    return false;

  Atom actual_type;
  int actual_format;
  unsigned long num_items, bytes_after;
  unsigned char* data;

  if (XIGetProperty(display, device_id, prop, 0, 1000, False, AnyPropertyType,
                    &actual_type, &actual_format, &num_items, &bytes_after,
                    &data) != Success) {
    // XIGetProperty can generate BadAtom, BadValue, and BadWindow errors
    fprintf(stderr, "Mysterious X server error\n");
    return false;
  }
  if (actual_type == None)
    return false;
  XFree(data);
  return (actual_type == XA_INTEGER);
}

int GetTouchpadXId(Display* display) {
  int num_devices = 0;
  XIDeviceInfo* info = XIQueryDevice(display, XIAllDevices, &num_devices);
  if (!info)
    PerrorAbort("XIQueryDevice");
  int ret = -1;
  for (int i = 0; i < num_devices; i++) {
    XIDeviceInfo* device = &info[i];
    if (device->use != XISlavePointer)
      continue;
    if (!IsCmtTouchpad(display, device->deviceid))
      continue;
    ret = device->deviceid;
    break;
  }
  XIFreeDeviceInfo(info);
  return ret;
}

class KeyboardMonitor {
 public:
  explicit KeyboardMonitor(const vector<unsigned short>& exclude)
      : exclude_(exclude) {}
  bool HandleInput(int fd, XNotifier* notifier);

 private:
  vector<unsigned short> exclude_;
};

bool KeyboardMonitor::HandleInput(int fd, XNotifier* notifier) {
  const size_t kNumEvents = 16;  // Max events to read per read call.
  struct input_event events[kNumEvents];
  const size_t kEventSize = sizeof(events[0]);
  while (true) {
    ssize_t readlen = read(fd, events, sizeof(events));
    if (readlen < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return true;
      if (errno == ENODEV)
        return false;  // Device is gone. Try to reopen
      PerrorAbort("read");
    }
    if (readlen == 0) {
      // EOF. Reopen device.
      return false;
    }
    if (readlen % kEventSize) {
      fprintf(stderr, "Read %zd bytes, not a multiple of %zu.\n",
              readlen, kEventSize);
      exit(1);
    }
    for (size_t i = 0, cnt = readlen / kEventSize; i < cnt; i++) {
      const struct input_event& event = events[i];
      if (event.type == EV_KEY && !VectorContainsKey(exclude_, event.code))
        if (!notifier->Notify(event.time))
          return false;
    }
  }
  return true;
}

class MainLoop {
 public:
  MainLoop(Display* display, KeyboardMonitor* monitor)
      : monitor_(monitor),
        display_(display),
        kb_fd_(-1),
        tp_id_(-1),
        x_fd_(ConnectionNumber(display_)),
        xiopcode_(GetXInputOpCode()) {}
  void Run() {
    XIEventMask evmask;
    unsigned char mask[XIMaskLen(XI_LASTEVENT)] = {0};

    XISetMask(mask, XI_HierarchyChanged);
    evmask.deviceid = XIAllDevices;
    evmask.mask_len = sizeof(mask);
    evmask.mask = mask;

    XISelectEvents(display_, DefaultRootWindow(display_), &evmask, 1);

    while (true) {
      if (DevicesNeedRefresh()) {
        sleep(1);
        RefreshDevices();
        continue;
      }
      int max_fd_plus_1 = std::max(x_fd_, kb_fd_) + 1;
      fd_set theset;
      FD_ZERO(&theset);
      FD_SET(x_fd_, &theset);
      FD_SET(kb_fd_, &theset);
      int rc = select(max_fd_plus_1, &theset, NULL, NULL, NULL);
      if (errno == EBADF) {
        close(kb_fd_);
        kb_fd_ = -1;
        continue;
      }
      if (rc < 0)
        PerrorAbort("select");
      if (FD_ISSET(x_fd_, &theset))
        ServiceXFd();
      else if (FD_ISSET(kb_fd_, &theset))
        ServiceKeyboardFd();
    }
  }
  void ServiceXFd() {
    while (XPending(display_)) {
      XEvent ev;
      XNextEvent(display_, &ev);
      if (ev.xcookie.type == GenericEvent &&
          ev.xcookie.extension == xiopcode_ &&
          XGetEventData(display_, &ev.xcookie)) {
        if (ev.xcookie.evtype == XI_HierarchyChanged) {
          // Rescan for touchpad
          tp_id_ = -1;
        }
        XFreeEventData(display_, &ev.xcookie);
      }
    }
  }
  void ServiceKeyboardFd() {
    if (!monitor_->HandleInput(kb_fd_, notifier_.get())) {
      close(kb_fd_);
      kb_fd_ = -1;
    }
  }
  void RefreshDevices() {
    if (kb_fd_ < 0)
      GetKeyboardFd();
    if (tp_id_ < 0) {
      tp_id_ = GetTouchpadXId(display_);
      if (tp_id_ >= 0)
        notifier_.reset(new XNotifier(display_, tp_id_));
    }
  }
  bool DevicesNeedRefresh() {
    return kb_fd_ < 0 || tp_id_ < 0;
  }
  void GetKeyboardFd() {
    string path = GetKeyboardPath(display_);
    if (path.empty()) {
      kb_fd_ = -1;
      return;
    }
    kb_fd_ = open(path.c_str(), O_RDONLY | O_NONBLOCK, 0);
    if (kb_fd_) {
      // Try to get monotonic clock events
      unsigned int clk = CLOCK_MONOTONIC;
      int rc = ioctl(kb_fd_, EVIOCSCLOCKID, &clk);
      if (rc < 0)
        fprintf(stderr, "Failed to switch to monotonic timestamps\n");
    }
  }

  // Lifted from http://codereview.chromium.org/6975057/patch/16001/17005
  int GetXInputOpCode() {
    static const char kExtensionName[] = "XInputExtension";
    int xi_opcode = -1;
    int event;
    int error;

    if (!XQueryExtension(display_, kExtensionName, &xi_opcode, &event,
                         &error)) {
      printf("X Input extension not available: error=%d\n", error);
      return -1;
    }
    return xi_opcode;
  }

 private:
  KeyboardMonitor* monitor_;
  Display* display_;
  scoped_ptr<XNotifier> notifier_;
  int kb_fd_;
  int tp_id_;
  int x_fd_;
  int xiopcode_;
};

};  // namespace keyboard_touchpad_helper

int main(int argc, char** argv) {
  using namespace keyboard_touchpad_helper;
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (!FLAGS_foreground && daemon(0, 0) < 0)
    PerrorAbort("daemon");

  Display* dpy = XOpenDisplay(
      FLAGS_display.empty() ? NULL : FLAGS_display.c_str());
  if (!dpy) {
    printf("XOpenDisplay failed\n");
    exit(1);
  }

  // Check for Xinput 2
  int opcode, event, err;
  if (!XQueryExtension(dpy, "XInputExtension", &opcode, &event, &err)) {
    fprintf(stderr,
            "Failed to get XInputExtension.\n");
    return -1;
  }

  int major = 2, minor = 0;
  if (XIQueryVersion(dpy, &major, &minor) == BadRequest) {
    fprintf(stderr,
            "Server does not have XInput2.\n");
    return -1;
  }

  KeyboardMonitor monitor(ParseExclusions());

  MainLoop looper(dpy, &monitor);
  looper.Run();
  return 0;
}
