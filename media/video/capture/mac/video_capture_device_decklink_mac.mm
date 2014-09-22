// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/mac/video_capture_device_decklink_mac.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/sys_string_conversions.h"
#include "third_party/decklink/mac/include/DeckLinkAPI.h"

namespace {

// DeckLink SDK uses ScopedComPtr-style APIs. Chrome ScopedComPtr is only
// available for Windows builds. This is a verbatim knock-off of the needed
// parts of base::win::ScopedComPtr<> for ref counting.
template <class T>
class ScopedDeckLinkPtr : public scoped_refptr<T> {
 public:
  using scoped_refptr<T>::ptr_;

  T** Receive() {
    DCHECK(!ptr_) << "Object leak. Pointer must be NULL";
    return &ptr_;
  }

  void** ReceiveVoid() {
    return reinterpret_cast<void**>(Receive());
  }

  void Release() {
    if (ptr_ != NULL) {
      ptr_->Release();
      ptr_ = NULL;
    }
  }
};

}  // namespace

namespace media {

std::string JoinDeviceNameAndFormat(CFStringRef name, CFStringRef format) {
  return base::SysCFStringRefToUTF8(name) + " - " +
      base::SysCFStringRefToUTF8(format);
}

//static
void VideoCaptureDeviceDeckLinkMac::EnumerateDevices(
    VideoCaptureDevice::Names* device_names) {
  scoped_refptr<IDeckLinkIterator> decklink_iter(
      CreateDeckLinkIteratorInstance());
  // At this point, not being able to create a DeckLink iterator means that
  // there are no Blackmagic devices in the system but this isn't an error.
  DVLOG_IF(1, !decklink_iter.get()) << "Could not create DeckLink iterator";
  if (!decklink_iter.get())
    return;

  ScopedDeckLinkPtr<IDeckLink> decklink;
  while (decklink_iter->Next(decklink.Receive()) == S_OK) {
    ScopedDeckLinkPtr<IDeckLink> decklink_local;
    decklink_local.swap(decklink);

    CFStringRef device_model_name = NULL;
    HRESULT hr = decklink_local->GetModelName(&device_model_name);
    DVLOG_IF(1, hr != S_OK) << "Error reading Blackmagic device model name";
    CFStringRef device_display_name = NULL;
    hr = decklink_local->GetDisplayName(&device_display_name);
    DVLOG_IF(1, hr != S_OK) << "Error reading Blackmagic device display name";
    DVLOG_IF(1, hr == S_OK) << "Blackmagic device found with name: " <<
        base::SysCFStringRefToUTF8(device_display_name);

    if (!device_model_name && !device_display_name)
      continue;

    ScopedDeckLinkPtr<IDeckLinkInput> decklink_input;
    if (decklink_local->QueryInterface(IID_IDeckLinkInput,
        decklink_input.ReceiveVoid()) != S_OK) {
      DLOG(ERROR) << "Error Blackmagic querying input interface.";
      return;
    }

    ScopedDeckLinkPtr<IDeckLinkDisplayModeIterator> display_mode_iter;
    if (decklink_input->GetDisplayModeIterator(display_mode_iter.Receive()) !=
        S_OK) {
      continue;
    }

    ScopedDeckLinkPtr<IDeckLinkDisplayMode> display_mode;
    while (display_mode_iter->Next(display_mode.Receive()) == S_OK) {
      CFStringRef format_name = NULL;
      if (display_mode->GetName(&format_name) == S_OK) {
        VideoCaptureDevice::Name name(
            JoinDeviceNameAndFormat(device_display_name, format_name),
            JoinDeviceNameAndFormat(device_model_name, format_name),
            VideoCaptureDevice::Name::DECKLINK,
            VideoCaptureDevice::Name::OTHER_TRANSPORT);
        device_names->push_back(name);
        DVLOG(1) << "Blackmagic camera enumerated: " << name.name();
      }
      display_mode.Release();
    }
  }
}

// static
void VideoCaptureDeviceDeckLinkMac::EnumerateDeviceCapabilities(
    const VideoCaptureDevice::Name& device,
    VideoCaptureFormats* supported_formats) {
  scoped_refptr<IDeckLinkIterator> decklink_iter(
      CreateDeckLinkIteratorInstance());
  DLOG_IF(ERROR, !decklink_iter.get()) << "Error creating DeckLink iterator";
  if (!decklink_iter.get())
    return;

  ScopedDeckLinkPtr<IDeckLink> decklink;
  while (decklink_iter->Next(decklink.Receive()) == S_OK) {
    ScopedDeckLinkPtr<IDeckLink> decklink_local;
    decklink_local.swap(decklink);

    ScopedDeckLinkPtr<IDeckLinkInput> decklink_input;
    if (decklink_local->QueryInterface(IID_IDeckLinkInput,
            decklink_input.ReceiveVoid()) != S_OK) {
      DLOG(ERROR) << "Error Blackmagic querying input interface.";
      return;
    }

    ScopedDeckLinkPtr<IDeckLinkDisplayModeIterator> display_mode_iter;
    if (decklink_input->GetDisplayModeIterator(display_mode_iter.Receive()) !=
        S_OK) {
      continue;
    }

    CFStringRef device_model_name = NULL;
    if (decklink_local->GetModelName(&device_model_name) != S_OK)
      continue;

    ScopedDeckLinkPtr<IDeckLinkDisplayMode> display_mode;
    while (display_mode_iter->Next(display_mode.Receive()) == S_OK) {
      CFStringRef format_name = NULL;
      if (display_mode->GetName(&format_name) == S_OK && device.id() !=
          JoinDeviceNameAndFormat(device_model_name, format_name)) {
        display_mode.Release();
        continue;
      }

      // IDeckLinkDisplayMode does not have information on pixel format, this
      // is only available on capture.
      media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_UNKNOWN;
      BMDTimeValue time_value, time_scale;
      float frame_rate = 0.0f;
      if (display_mode->GetFrameRate(&time_value, &time_scale) == S_OK &&
          time_value > 0) {
        frame_rate = static_cast<float>(time_scale) / time_value;
      }
      media::VideoCaptureFormat format(
          gfx::Size(display_mode->GetWidth(), display_mode->GetHeight()),
          frame_rate,
          pixel_format);
      supported_formats->push_back(format);
      DVLOG(2) << device.name() << " " << format.ToString();
      display_mode.Release();
    }
    return;
  }
}

VideoCaptureDeviceDeckLinkMac::VideoCaptureDeviceDeckLinkMac(
    const Name& device_name) {}

VideoCaptureDeviceDeckLinkMac::~VideoCaptureDeviceDeckLinkMac() {}

void VideoCaptureDeviceDeckLinkMac::AllocateAndStart(
    const VideoCaptureParams& params,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  NOTIMPLEMENTED();
}

void VideoCaptureDeviceDeckLinkMac::StopAndDeAllocate() {
  NOTIMPLEMENTED();
}

} // namespace media
