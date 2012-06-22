// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "chrome/browser/speech/speech_recognition_bubble.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skbitmap_operations.h"

using content::WebContents;

namespace {

const color_utils::HSL kGrayscaleShift = { -1, 0, 0.6 };
const int kWarmingUpAnimationStartMs = 500;
const int kWarmingUpAnimationStepMs = 100;
const int kRecognizingAnimationStepMs = 100;

// A lazily initialized singleton to hold all the image used by the speech
// recognition bubbles and safely destroy them on exit.
class SpeechRecognitionBubbleImages {
 public:
  const std::vector<SkBitmap>& spinner() { return spinner_; }
  const std::vector<SkBitmap>& warm_up() { return warm_up_; }
  gfx::ImageSkia* mic_full() { return mic_full_; }
  gfx::ImageSkia* mic_empty() { return mic_empty_; }
  gfx::ImageSkia* mic_noise() { return mic_noise_; }
  gfx::ImageSkia* mic_mask() { return mic_mask_; }

 private:
  // Private constructor to enforce singleton.
  friend struct base::DefaultLazyInstanceTraits<SpeechRecognitionBubbleImages>;
  SpeechRecognitionBubbleImages();

  std::vector<SkBitmap> spinner_;  // Frames for the progress spinner.
  std::vector<SkBitmap> warm_up_;  // Frames for the warm up animation.

  // These images are owned by ResourceBundle and need not be destroyed.
  gfx::ImageSkia* mic_full_;  // Mic image with full volume.
  gfx::ImageSkia* mic_noise_;  // Mic image with full noise volume.
  gfx::ImageSkia* mic_empty_;  // Mic image with zero volume.
  gfx::ImageSkia* mic_mask_;  // Gradient mask used by the volume indicator.
};

SpeechRecognitionBubbleImages::SpeechRecognitionBubbleImages() {
  mic_empty_ = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_SPEECH_INPUT_MIC_EMPTY);
  mic_noise_ = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_SPEECH_INPUT_MIC_NOISE);
  mic_full_ = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_SPEECH_INPUT_MIC_FULL);
  mic_mask_ = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_SPEECH_INPUT_MIC_MASK);

  // The sprite image consists of all the animation frames put together in one
  // horizontal/wide image. Each animation frame is square in shape within the
  // sprite.
  const SkBitmap* spinner_image = ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_SPEECH_INPUT_SPINNER)->bitmap();
  int frame_size = spinner_image->height();

  // When recording starts up, it may take a short while (few ms or even a
  // couple of seconds) before the audio device starts really capturing data.
  // This is more apparent on first use. To cover such cases we show a warming
  // up state in the bubble starting with a blank spinner image. If audio data
  // starts coming in within a couple hundred ms, we switch to the recording
  // UI and if it takes longer, we show the real warm up animation frames.
  // This reduces visual jank for the most part.
  SkBitmap empty_spinner;
  empty_spinner.setConfig(SkBitmap::kARGB_8888_Config, frame_size, frame_size);
  empty_spinner.allocPixels();
  empty_spinner.eraseRGB(255, 255, 255);
  warm_up_.push_back(empty_spinner);

  for (SkIRect src_rect(SkIRect::MakeWH(frame_size, frame_size));
       src_rect.fLeft < spinner_image->width();
       src_rect.offset(frame_size, 0)) {
    SkBitmap frame;
    spinner_image->extractSubset(&frame, src_rect);

    // The image created by extractSubset just points to the same pixels as
    // the original and adjusts rowBytes accordingly. However that doesn't
    // render properly and gets vertically squished in Linux due to a bug in
    // Skia. Until that gets fixed we work around by taking a real copy of it
    // below as the copied image has the correct rowBytes and renders fine.
    SkBitmap frame_copy;
    frame.copyTo(&frame_copy, SkBitmap::kARGB_8888_Config);
    spinner_.push_back(frame_copy);

    // The warm up spinner animation is a gray scale version of the real one.
    warm_up_.push_back(SkBitmapOperations::CreateHSLShiftedBitmap(
        frame_copy, kGrayscaleShift));
  }
}

base::LazyInstance<SpeechRecognitionBubbleImages> g_images =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

SpeechRecognitionBubble::FactoryMethod SpeechRecognitionBubble::factory_ = NULL;
const int SpeechRecognitionBubble::kBubbleTargetOffsetX = 10;

SpeechRecognitionBubble* SpeechRecognitionBubble::Create(
    WebContents* web_contents, Delegate* delegate,
    const gfx::Rect& element_rect) {
  if (factory_)
    return (*factory_)(web_contents, delegate, element_rect);

  // Has the tab already closed before bubble create request was processed?
  if (!web_contents)
    return NULL;

  return CreateNativeBubble(web_contents, delegate, element_rect);
}

SpeechRecognitionBubbleBase::SpeechRecognitionBubbleBase(
    WebContents* web_contents)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      display_mode_(DISPLAY_MODE_RECORDING),
      web_contents_(web_contents) {
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

SpeechRecognitionBubbleBase::~SpeechRecognitionBubbleBase() {
  // This destructor is added to make sure members such as the scoped_ptr
  // get destroyed here and the derived classes don't have to care about such
  // member variables which they don't use.
}

void SpeechRecognitionBubbleBase::SetWarmUpMode() {
  weak_factory_.InvalidateWeakPtrs();
  display_mode_ = DISPLAY_MODE_WARM_UP;
  animation_step_ = 0;
  DoWarmingUpAnimationStep();
  UpdateLayout();
}

void SpeechRecognitionBubbleBase::DoWarmingUpAnimationStep() {
  SetImage(g_images.Get().warm_up()[animation_step_]);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SpeechRecognitionBubbleBase::DoWarmingUpAnimationStep,
          weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(
          animation_step_ == 0 ? kWarmingUpAnimationStartMs
                               : kWarmingUpAnimationStepMs));
  if (++animation_step_ >= static_cast<int>(g_images.Get().warm_up().size()))
    animation_step_ = 1;  // Frame 0 is skipped during the animation.
}

void SpeechRecognitionBubbleBase::SetRecordingMode() {
  weak_factory_.InvalidateWeakPtrs();
  display_mode_ = DISPLAY_MODE_RECORDING;
  SetInputVolume(0, 0);
  UpdateLayout();
}

void SpeechRecognitionBubbleBase::SetRecognizingMode() {
  display_mode_ = DISPLAY_MODE_RECOGNIZING;
  animation_step_ = 0;
  DoRecognizingAnimationStep();
  UpdateLayout();
}

void SpeechRecognitionBubbleBase::DoRecognizingAnimationStep() {
  SetImage(g_images.Get().spinner()[animation_step_]);
  if (++animation_step_ >= static_cast<int>(g_images.Get().spinner().size()))
    animation_step_ = 0;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SpeechRecognitionBubbleBase::DoRecognizingAnimationStep,
          weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kRecognizingAnimationStepMs));
}

void SpeechRecognitionBubbleBase::SetMessage(const string16& text) {
  weak_factory_.InvalidateWeakPtrs();
  message_text_ = text;
  display_mode_ = DISPLAY_MODE_MESSAGE;
  UpdateLayout();
}

void SpeechRecognitionBubbleBase::DrawVolumeOverlay(SkCanvas* canvas,
                                                    const gfx::ImageSkia& image,
                                                    float volume) {
  buffer_image_->eraseARGB(0, 0, 0, 0);

  int width = mic_image_->width();
  int height = mic_image_->height();
  SkCanvas buffer_canvas(*buffer_image_);

  buffer_canvas.save();
  const int kVolumeSteps = 12;
  SkScalar clip_right =
      (((1.0f - volume) * (width * (kVolumeSteps + 1))) - width) / kVolumeSteps;
  buffer_canvas.clipRect(SkRect::MakeLTRB(0, 0,
      SkIntToScalar(width) - clip_right, SkIntToScalar(height)));
  buffer_canvas.drawBitmap(image, 0, 0);
  buffer_canvas.restore();
  SkPaint multiply_paint;
  multiply_paint.setXfermode(SkXfermode::Create(SkXfermode::kMultiply_Mode));
  buffer_canvas.drawBitmap(*g_images.Get().mic_mask(), -clip_right, 0,
                           &multiply_paint);

  canvas->drawBitmap(*buffer_image_.get(), 0, 0);
}

void SpeechRecognitionBubbleBase::SetInputVolume(float volume,
                                                 float noise_volume) {
  mic_image_->eraseARGB(0, 0, 0, 0);
  SkCanvas canvas(*mic_image_);

  // Draw the empty volume image first and the current volume image on top,
  // and then the noise volume image on top of both.
  canvas.drawBitmap(*g_images.Get().mic_empty(), 0, 0);
  DrawVolumeOverlay(&canvas, *g_images.Get().mic_full(), volume);
  DrawVolumeOverlay(&canvas, *g_images.Get().mic_noise(), noise_volume);

  SetImage(*mic_image_.get());
}

WebContents* SpeechRecognitionBubbleBase::GetWebContents() {
  return web_contents_;
}

void SpeechRecognitionBubbleBase::SetImage(const gfx::ImageSkia& image) {
  icon_image_.reset(new gfx::ImageSkia(image));
  UpdateImage();
}

gfx::ImageSkia SpeechRecognitionBubbleBase::icon_image() {
  return (icon_image_ != NULL) ? *icon_image_ : gfx::ImageSkia();
}
