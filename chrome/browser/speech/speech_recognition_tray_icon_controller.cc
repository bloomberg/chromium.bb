// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_tray_icon_controller.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skbitmap_operations.h"

using content::BrowserThread;

namespace {

// Number of volume steps used when rendering the VU meter icon.
const int kVolumeSteps = 6;

// A lazily initialized singleton to hold all the images used by the
// notification icon and safely destroy them on exit.
class NotificationTrayImages {
 public:
  SkBitmap* mic_full() { return mic_full_; }
  SkBitmap* mic_empty() { return mic_empty_; }
  SkBitmap* balloon_icon() { return balloon_icon_; }

 private:
  // Private constructor to enforce singleton.
  friend struct base::DefaultLazyInstanceTraits<NotificationTrayImages>;
  NotificationTrayImages();

  // These bitmaps are owned by ResourceBundle and need not be destroyed.
  SkBitmap* mic_full_; // Tray mic image with full volume.
  SkBitmap* mic_empty_; // Tray mic image with zero volume.
  SkBitmap* balloon_icon_; // High resolution mic for the notification balloon.
};

NotificationTrayImages::NotificationTrayImages() {
  mic_empty_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_SPEECH_INPUT_TRAY_MIC_EMPTY);
  mic_full_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_SPEECH_INPUT_TRAY_MIC_FULL);
  balloon_icon_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_SPEECH_INPUT_TRAY_BALLOON_ICON);
}

base::LazyInstance<NotificationTrayImages> g_images = LAZY_INSTANCE_INITIALIZER;

}  // namespace

SpeechRecognitionTrayIconController::SpeechRecognitionTrayIconController()
    : tray_icon_(NULL) {
  Initialize();
}

SpeechRecognitionTrayIconController::~SpeechRecognitionTrayIconController() {
  DCHECK(!tray_icon_);
}

void SpeechRecognitionTrayIconController::Show(const string16& tooltip,
                                               bool show_balloon) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SpeechRecognitionTrayIconController::Show, this,
                   tooltip, show_balloon));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (StatusTray* status_tray = g_browser_process->status_tray()) {
    DCHECK(!tray_icon_);
    AddRef();  // Balanced in Hide().
    tray_icon_ = status_tray->CreateStatusIcon();
    DCHECK(tray_icon_);
    VLOG(1) << "Tray icon added.";

    SetVUMeterVolume(0.0f);
    tray_icon_->SetToolTip(l10n_util::GetStringFUTF16(
        IDS_SPEECH_INPUT_TRAY_TOOLTIP,
        tooltip,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  } else {
    VLOG(1) << "This platform doesn't support notification icons.";
    return;
  }

  if (show_balloon)
    ShowNotificationBalloon(tooltip);
}

void SpeechRecognitionTrayIconController::Hide() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SpeechRecognitionTrayIconController::Hide, this));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!tray_icon_)
    return;

  if (StatusTray* status_tray = g_browser_process->status_tray()) {
    status_tray->RemoveStatusIcon(tray_icon_);
    tray_icon_ = NULL;
    VLOG(1) << "Tray icon removed.";
    Release();  // Balanced in Show().
  }
}

void SpeechRecognitionTrayIconController::SetVUMeterVolume(float volume) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SpeechRecognitionTrayIconController::SetVUMeterVolume, this,
                   volume));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!tray_icon_)
    return;

  mic_image_->eraseARGB(0, 0, 0, 0);
  SkCanvas canvas(*mic_image_);

  // Draw the empty volume image first and the current volume image on top.
  canvas.drawBitmap(*g_images.Get().mic_empty(), 0, 0);
  DrawVolume(&canvas, *g_images.Get().mic_full(), volume);

  tray_icon_->SetImage(*mic_image_.get());
}

void SpeechRecognitionTrayIconController::Initialize() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SpeechRecognitionTrayIconController::Initialize, this));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  mic_image_.reset(new SkBitmap());
  mic_image_->setConfig(SkBitmap::kARGB_8888_Config,
                        g_images.Get().mic_empty()->width(),
                        g_images.Get().mic_empty()->height());
  mic_image_->allocPixels();

  buffer_image_.reset(new SkBitmap());
  buffer_image_->setConfig(SkBitmap::kARGB_8888_Config,
                           g_images.Get().mic_empty()->width(),
                           g_images.Get().mic_empty()->height());
  buffer_image_->allocPixels();
}

void SpeechRecognitionTrayIconController::DrawVolume(
    SkCanvas* canvas,
    const SkBitmap& bitmap,
    float volume) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  buffer_image_->eraseARGB(0, 0, 0, 0);

  int width = mic_image_->width();
  int height = mic_image_->height();
  SkCanvas buffer_canvas(*buffer_image_);

  SkScalar clip_top =
      (((1.0f - volume) * (height * (kVolumeSteps + 1))) - height) /
      kVolumeSteps;
  buffer_canvas.clipRect(SkRect::MakeLTRB(0, clip_top,
      SkIntToScalar(width), SkIntToScalar(height)));
  buffer_canvas.drawBitmap(bitmap, 0, 0);

  canvas->drawBitmap(*buffer_image_.get(), 0, 0);
}

void SpeechRecognitionTrayIconController::ShowNotificationBalloon(
    const string16& text) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  string16 title = l10n_util::GetStringUTF16(
      IDS_SPEECH_INPUT_TRAY_BALLOON_TITLE);
  string16 message = l10n_util::GetStringFUTF16(
          IDS_SPEECH_INPUT_TRAY_BALLOON_BODY,
          text,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  tray_icon_->DisplayBalloon(*g_images.Get().balloon_icon(), title, message);
}
