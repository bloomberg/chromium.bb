// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_FLAGS_H_
#define CC_PAINT_PAINT_FLAGS_H_

#include "base/compiler_specific.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPathEffect.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace cc {

class CC_PAINT_EXPORT PaintFlags {
 public:
  enum Style {
    kFill_Style = SkPaint::kFill_Style,
    kStroke_Style = SkPaint::kStroke_Style,
    kStrokeAndFill_Style = SkPaint::kStrokeAndFill_Style,
  };
  ALWAYS_INLINE Style getStyle() const {
    return static_cast<Style>(paint_.getStyle());
  }
  ALWAYS_INLINE void setStyle(Style style) {
    paint_.setStyle(static_cast<SkPaint::Style>(style));
  }
  ALWAYS_INLINE SkColor getColor() const { return paint_.getColor(); }
  ALWAYS_INLINE void setColor(SkColor color) { paint_.setColor(color); }
  ALWAYS_INLINE void setARGB(U8CPU a, U8CPU r, U8CPU g, U8CPU b) {
    paint_.setARGB(a, r, g, b);
  }
  ALWAYS_INLINE uint8_t getAlpha() const { return paint_.getAlpha(); }
  ALWAYS_INLINE void setAlpha(U8CPU a) { paint_.setAlpha(a); }
  ALWAYS_INLINE void setBlendMode(SkBlendMode mode) {
    paint_.setBlendMode(mode);
  }
  ALWAYS_INLINE SkBlendMode getBlendMode() const {
    return paint_.getBlendMode();
  }
  ALWAYS_INLINE bool isSrcOver() const { return paint_.isSrcOver(); }
  ALWAYS_INLINE bool isAntiAlias() const { return paint_.isAntiAlias(); }
  ALWAYS_INLINE void setAntiAlias(bool aa) { paint_.setAntiAlias(aa); }
  ALWAYS_INLINE bool isVerticalText() const { return paint_.isVerticalText(); }
  ALWAYS_INLINE void setVerticalText(bool vertical) {
    paint_.setVerticalText(vertical);
  }
  ALWAYS_INLINE bool isSubpixelText() const { return paint_.isSubpixelText(); }
  ALWAYS_INLINE void setSubpixelText(bool subpixel_text) {
    paint_.setSubpixelText(subpixel_text);
  }
  ALWAYS_INLINE bool isLCDRenderText() const {
    return paint_.isLCDRenderText();
  }
  ALWAYS_INLINE void setLCDRenderText(bool lcd_text) {
    paint_.setLCDRenderText(lcd_text);
  }
  enum Hinting {
    kNo_Hinting = SkPaint::kNo_Hinting,
    kSlight_Hinting = SkPaint::kSlight_Hinting,
    kNormal_Hinting = SkPaint::kNormal_Hinting,  //!< this is the default
    kFull_Hinting = SkPaint::kFull_Hinting
  };
  ALWAYS_INLINE Hinting getHinting() const {
    return static_cast<Hinting>(paint_.getHinting());
  }
  ALWAYS_INLINE void setHinting(Hinting hinting_level) {
    paint_.setHinting(static_cast<SkPaint::Hinting>(hinting_level));
  }
  ALWAYS_INLINE bool isAutohinted() const { return paint_.isAutohinted(); }
  ALWAYS_INLINE void setAutohinted(bool use_auto_hinter) {
    paint_.setAutohinted(use_auto_hinter);
  }
  ALWAYS_INLINE bool isDither() const { return paint_.isDither(); }
  ALWAYS_INLINE void setDither(bool dither) { paint_.setDither(dither); }
  enum TextEncoding {
    kUTF8_TextEncoding = SkPaint::kUTF8_TextEncoding,
    kUTF16_TextEncoding = SkPaint::kUTF16_TextEncoding,
    kUTF32_TextEncoding = SkPaint::kUTF32_TextEncoding,
    kGlyphID_TextEncoding = SkPaint::kGlyphID_TextEncoding
  };
  ALWAYS_INLINE TextEncoding getTextEncoding() const {
    return static_cast<TextEncoding>(paint_.getTextEncoding());
  }
  ALWAYS_INLINE void setTextEncoding(TextEncoding encoding) {
    paint_.setTextEncoding(static_cast<SkPaint::TextEncoding>(encoding));
  }
  ALWAYS_INLINE SkScalar getTextSize() const { return paint_.getTextSize(); }
  ALWAYS_INLINE void setTextSize(SkScalar textSize) {
    paint_.setTextSize(textSize);
  }
  ALWAYS_INLINE void setFilterQuality(SkFilterQuality quality) {
    paint_.setFilterQuality(quality);
  }
  ALWAYS_INLINE SkFilterQuality getFilterQuality() const {
    return paint_.getFilterQuality();
  }
  ALWAYS_INLINE SkScalar getStrokeWidth() const {
    return paint_.getStrokeWidth();
  }
  ALWAYS_INLINE void setStrokeWidth(SkScalar width) {
    paint_.setStrokeWidth(width);
  }
  ALWAYS_INLINE SkScalar getStrokeMiter() const {
    return paint_.getStrokeMiter();
  }
  ALWAYS_INLINE void setStrokeMiter(SkScalar miter) {
    paint_.setStrokeMiter(miter);
  }
  enum Cap {
    kButt_Cap = SkPaint::kButt_Cap,    //!< begin/end contours with no extension
    kRound_Cap = SkPaint::kRound_Cap,  //!< begin/end contours with a
                                       //! semi-circle extension
    kSquare_Cap = SkPaint::kSquare_Cap,  //!< begin/end contours with a half
                                         //! square extension
    kLast_Cap = kSquare_Cap,
    kDefault_Cap = kButt_Cap
  };
  ALWAYS_INLINE Cap getStrokeCap() const {
    return static_cast<Cap>(paint_.getStrokeCap());
  }
  ALWAYS_INLINE void setStrokeCap(Cap cap) {
    paint_.setStrokeCap(static_cast<SkPaint::Cap>(cap));
  }
  enum Join {
    kMiter_Join = SkPaint::kMiter_Join,
    kRound_Join = SkPaint::kRound_Join,
    kBevel_Join = SkPaint::kBevel_Join,
    kLast_Join = kBevel_Join,
    kDefault_Join = kMiter_Join
  };
  ALWAYS_INLINE Join getStrokeJoin() const {
    return static_cast<Join>(paint_.getStrokeJoin());
  }
  ALWAYS_INLINE void setStrokeJoin(Join join) {
    paint_.setStrokeJoin(static_cast<SkPaint::Join>(join));
  }
  ALWAYS_INLINE SkTypeface* getTypeface() const { return paint_.getTypeface(); }
  ALWAYS_INLINE void setTypeface(sk_sp<SkTypeface> typeface) {
    paint_.setTypeface(std::move(typeface));
  }
  ALWAYS_INLINE SkColorFilter* getColorFilter() const {
    return paint_.getColorFilter();
  }
  ALWAYS_INLINE sk_sp<SkColorFilter> refColorFilter() const {
    return paint_.refColorFilter();
  }
  ALWAYS_INLINE void setColorFilter(sk_sp<SkColorFilter> filter) {
    paint_.setColorFilter(std::move(filter));
  }
  ALWAYS_INLINE SkMaskFilter* getMaskFilter() const {
    return paint_.getMaskFilter();
  }
  ALWAYS_INLINE void setMaskFilter(sk_sp<SkMaskFilter> mask) {
    paint_.setMaskFilter(std::move(mask));
  }
  // TODO(vmpstr): Remove this from recording calls, since we want to avoid
  // constructing the shader until rasterization.
  ALWAYS_INLINE SkShader* getSkShader() const { return paint_.getShader(); }

  // Returns true if the shader has been set on the flags.
  ALWAYS_INLINE bool HasShader() const { return !!paint_.getShader(); }

  // Returns whether the shader is opaque. Note that it is only valid to call
  // this function if HasShader() returns true.
  ALWAYS_INLINE bool ShaderIsOpaque() const {
    return paint_.getShader()->isOpaque();
  }

  ALWAYS_INLINE void setShader(std::unique_ptr<PaintShader> shader) {
    paint_.setShader(shader ? shader->sk_shader() : nullptr);
  }
  ALWAYS_INLINE SkPathEffect* getPathEffect() const {
    return paint_.getPathEffect();
  }
  ALWAYS_INLINE void setPathEffect(sk_sp<SkPathEffect> effect) {
    paint_.setPathEffect(std::move(effect));
  }
  ALWAYS_INLINE bool getFillPath(const SkPath& src,
                                 SkPath* dst,
                                 const SkRect* cullRect = nullptr,
                                 SkScalar resScale = 1) const {
    return paint_.getFillPath(src, dst, cullRect, resScale);
  }
  ALWAYS_INLINE sk_sp<SkImageFilter> refImageFilter() const {
    return paint_.refImageFilter();
  }
  ALWAYS_INLINE SkImageFilter* getImageFilter() const {
    return paint_.getImageFilter();
  }
  void setImageFilter(sk_sp<SkImageFilter> filter) {
    paint_.setImageFilter(std::move(filter));
  }
  ALWAYS_INLINE SkDrawLooper* getDrawLooper() const {
    return paint_.getDrawLooper();
  }
  ALWAYS_INLINE SkDrawLooper* getLooper() const { return paint_.getLooper(); }
  ALWAYS_INLINE void setLooper(sk_sp<SkDrawLooper> looper) {
    paint_.setLooper(std::move(looper));
  }
  ALWAYS_INLINE bool canComputeFastBounds() const {
    return paint_.canComputeFastBounds();
  }
  ALWAYS_INLINE const SkRect& computeFastBounds(const SkRect& orig,
                                                SkRect* storage) const {
    return paint_.computeFastBounds(orig, storage);
  }

  bool operator==(const PaintFlags& flags) { return flags.paint_ == paint_; }
  bool operator!=(const PaintFlags& flags) { return flags.paint_ != paint_; }

  // Returns true if this just represents an opacity blend when
  // used as saveLayer flags.
  bool IsSimpleOpacity() const;
  bool SupportsFoldingAlpha() const;

 private:
  friend const SkPaint& ToSkPaint(const PaintFlags& flags);
  friend const SkPaint* ToSkPaint(const PaintFlags* flags);

  SkPaint paint_;
};

ALWAYS_INLINE const SkPaint& ToSkPaint(const PaintFlags& flags) {
  return flags.paint_;
}

ALWAYS_INLINE const SkPaint* ToSkPaint(const PaintFlags* flags) {
  return &flags->paint_;
}

}  // namespace cc

#endif  // CC_PAINT_PAINT_FLAGS_H_
