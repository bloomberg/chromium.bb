// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_BUFFER_H_
#define CC_PAINT_PAINT_OP_BUFFER_H_

#include <stdint.h>

#include <string>
#include <type_traits>

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/optional.h"
#include "cc/base/math_util.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkTextBlob.h"

// PaintOpBuffer is a reimplementation of SkLiteDL.
// See: third_party/skia/src/core/SkLiteDL.h.

namespace cc {
class ImageDecodeCache;

class CC_PAINT_EXPORT ThreadsafeMatrix : public SkMatrix {
 public:
  explicit ThreadsafeMatrix(const SkMatrix& matrix) : SkMatrix(matrix) {
    (void)getType();
  }
};

class CC_PAINT_EXPORT ThreadsafePath : public SkPath {
 public:
  explicit ThreadsafePath(const SkPath& path) : SkPath(path) {
    updateBoundsCache();
  }
  ThreadsafePath() { updateBoundsCache(); }
};

// See PaintOp::Serialize/Deserialize for comments.  Derived Serialize types
// don't write the 4 byte type/skip header because they don't know how much
// data they will need to write.  PaintOp::Serialize itself must update it.
#define HAS_SERIALIZATION_FUNCTIONS()                                        \
  static size_t Serialize(const PaintOp* op, void* memory, size_t size,      \
                          const SerializeOptions& options);                  \
  static PaintOp* Deserialize(const volatile void* input, size_t input_size, \
                              void* output, size_t output_size);

enum class PaintOpType : uint8_t {
  Annotate,
  ClipPath,
  ClipRect,
  ClipRRect,
  Concat,
  DrawColor,
  DrawDRRect,
  DrawImage,
  DrawImageRect,
  DrawIRect,
  DrawLine,
  DrawOval,
  DrawPath,
  DrawRecord,
  DrawRect,
  DrawRRect,
  DrawTextBlob,
  Noop,
  Restore,
  Rotate,
  Save,
  SaveLayer,
  SaveLayerAlpha,
  Scale,
  SetMatrix,
  Translate,
  LastPaintOpType = Translate,
};

CC_PAINT_EXPORT std::string PaintOpTypeToString(PaintOpType type);

struct CC_PAINT_EXPORT PlaybackParams {
  PlaybackParams(ImageProvider* image_provider, const SkMatrix& original_ctm);
  ImageProvider* image_provider;
  const SkMatrix original_ctm;
};

class CC_PAINT_EXPORT PaintOp {
 public:
  uint32_t type : 8;
  uint32_t skip : 24;

  PaintOpType GetType() const { return static_cast<PaintOpType>(type); }

  // Subclasses should provide a static Raster() method which is called from
  // here. The Raster method should take a const PaintOp* parameter. It is
  // static with a pointer to the base type so that we can use it as a function
  // pointer.
  void Raster(SkCanvas* canvas, const PlaybackParams& params) const;
  bool IsDrawOp() const;
  bool IsPaintOpWithFlags() const;
  bool IsValid() const;

  struct SerializeOptions {
    ImageDecodeCache* decode_cache = nullptr;
  };

  // Subclasses should provide a static Serialize() method called from here.
  // If the op can be serialized to |memory| in no more than |size| bytes,
  // then return the number of bytes written.  If it won't fit, return 0.
  size_t Serialize(void* memory,
                   size_t size,
                   const SerializeOptions& options) const;

  // Deserializes a PaintOp of this type from a given buffer |input| of
  // at most |input_size| bytes.  Returns null on any errors.
  // The PaintOp is deserialized into the |output| buffer and returned
  // if valid.  nullptr is returned if the deserialization fails.
  // |output_size| must be at least LargestPaintOp + serialized->skip,
  // to fit all ops.  The caller is responsible for destroying these ops.
  // After reading, it returns the number of bytes read in |read_bytes|.
  static PaintOp* Deserialize(const volatile void* input,
                              size_t input_size,
                              void* output,
                              size_t output_size,
                              size_t* read_bytes);

  // For draw ops, returns true if a conservative bounding rect can be provided
  // for the op.
  static bool GetBounds(const PaintOp* op, SkRect* rect);

  // Returns true if executing this op will require decoding of any lazy
  // generated images.
  static bool OpHasDiscardableImages(const PaintOp* op);

  int CountSlowPaths() const { return 0; }
  int CountSlowPathsFromFlags() const { return 0; }

  bool HasNonAAPaint() const { return false; }

  bool HasDiscardableImages() const { return false; }
  bool HasDiscardableImagesFromFlags() const { return false; }

  // Returns the number of bytes used by this op in referenced sub records
  // and display lists.  This doesn't count other objects like paths or blobs.
  size_t AdditionalBytesUsed() const { return 0; }

  // Run the destructor for the derived op type.  Ops are usually contained in
  // memory buffers and so don't have their destructors run automatically.
  void DestroyThis();

  // DrawColor is more restrictive on the blend modes that can be used.
  static bool IsValidDrawColorSkBlendMode(SkBlendMode mode) {
    return static_cast<uint32_t>(mode) <=
           static_cast<uint32_t>(SkBlendMode::kLastCoeffMode);
  }

  // PaintFlags can have more complex blend modes than DrawColor.
  static bool IsValidPaintFlagsSkBlendMode(SkBlendMode mode) {
    return static_cast<uint32_t>(mode) <=
           static_cast<uint32_t>(SkBlendMode::kLastMode);
  }

  static bool IsValidSkClipOp(SkClipOp op) {
    return static_cast<uint32_t>(op) <=
           static_cast<uint32_t>(SkClipOp::kMax_EnumValue);
  }

  static bool IsValidPath(const SkPath& path) {
    return path.isValid() && path.pathRefIsValid();
  }

  static bool IsUnsetRect(const SkRect& rect) {
    return rect.fLeft == SK_ScalarInfinity;
  }
  static bool IsValidOrUnsetRect(const SkRect& rect) {
    return IsUnsetRect(rect) || rect.isFinite();
  }

  static constexpr bool kIsDrawOp = false;
  static constexpr bool kHasPaintFlags = false;
  // Since skip and type fit in a uint32_t, this is the max size of skip.
  static constexpr size_t kMaxSkip = static_cast<size_t>(1 << 24);
  static const SkRect kUnsetRect;
};

class CC_PAINT_EXPORT PaintOpWithFlags : public PaintOp {
 public:
  static constexpr bool kHasPaintFlags = true;
  explicit PaintOpWithFlags(const PaintFlags& flags) : flags(flags) {}

  int CountSlowPathsFromFlags() const { return flags.getPathEffect() ? 1 : 0; }
  bool HasNonAAPaint() const { return !flags.isAntiAlias(); }
  bool HasDiscardableImagesFromFlags() const;

  void RasterWithFlags(SkCanvas* canvas,
                       const PaintFlags* flags,
                       const PlaybackParams& params) const;

  // Subclasses should provide a static RasterWithFlags() method which is called
  // from the Raster() method. The RasterWithFlags() should use the SkPaint
  // passed to it, instead of the |flags| member directly, as some callers may
  // provide a modified PaintFlags. The RasterWithFlags() method is static with
  // a const PaintOpWithFlags* parameter so that it can be used as a function
  // pointer.
  PaintFlags flags;

 protected:
  PaintOpWithFlags() = default;
};

class CC_PAINT_EXPORT AnnotateOp final : public PaintOp {
 public:
  enum class AnnotationType {
    URL,
    LinkToDestination,
    NamedDestination,
  };

  static constexpr PaintOpType kType = PaintOpType::Annotate;
  AnnotateOp(PaintCanvas::AnnotationType annotation_type,
             const SkRect& rect,
             sk_sp<SkData> data);
  ~AnnotateOp();
  static void Raster(const AnnotateOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return rect.isFinite(); }
  HAS_SERIALIZATION_FUNCTIONS();

  PaintCanvas::AnnotationType annotation_type;
  SkRect rect;
  sk_sp<SkData> data;

 private:
  AnnotateOp();
};

class CC_PAINT_EXPORT ClipPathOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::ClipPath;
  ClipPathOp(SkPath path, SkClipOp op, bool antialias)
      : path(path), op(op), antialias(antialias) {}
  static void Raster(const ClipPathOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidSkClipOp(op) && IsValidPath(path); }
  int CountSlowPaths() const;
  bool HasNonAAPaint() const { return !antialias; }
  HAS_SERIALIZATION_FUNCTIONS();

  ThreadsafePath path;
  SkClipOp op;
  bool antialias;

 private:
  ClipPathOp() = default;
};

class CC_PAINT_EXPORT ClipRectOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::ClipRect;
  ClipRectOp(const SkRect& rect, SkClipOp op, bool antialias)
      : rect(rect), op(op), antialias(antialias) {}
  static void Raster(const ClipRectOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidSkClipOp(op) && rect.isFinite(); }
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect rect;
  SkClipOp op;
  bool antialias;

 private:
  ClipRectOp() = default;
};

class CC_PAINT_EXPORT ClipRRectOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::ClipRRect;
  ClipRRectOp(const SkRRect& rrect, SkClipOp op, bool antialias)
      : rrect(rrect), op(op), antialias(antialias) {}
  static void Raster(const ClipRRectOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidSkClipOp(op) && rrect.isValid(); }
  bool HasNonAAPaint() const { return !antialias; }
  HAS_SERIALIZATION_FUNCTIONS();

  SkRRect rrect;
  SkClipOp op;
  bool antialias;

 private:
  ClipRRectOp() = default;
};

class CC_PAINT_EXPORT ConcatOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Concat;
  explicit ConcatOp(const SkMatrix& matrix) : matrix(matrix) {}
  static void Raster(const ConcatOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  HAS_SERIALIZATION_FUNCTIONS();

  ThreadsafeMatrix matrix;

 private:
  ConcatOp() = default;
};

class CC_PAINT_EXPORT DrawColorOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawColor;
  static constexpr bool kIsDrawOp = true;
  DrawColorOp(SkColor color, SkBlendMode mode) : color(color), mode(mode) {}
  static void Raster(const DrawColorOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidDrawColorSkBlendMode(mode); }
  HAS_SERIALIZATION_FUNCTIONS();

  SkColor color;
  SkBlendMode mode;

 private:
  DrawColorOp() = default;
};

class CC_PAINT_EXPORT DrawDRRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawDRRect;
  static constexpr bool kIsDrawOp = true;
  DrawDRRectOp(const SkRRect& outer,
               const SkRRect& inner,
               const PaintFlags& flags)
      : PaintOpWithFlags(flags), outer(outer), inner(inner) {}
  static void RasterWithFlags(const DrawDRRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const {
    return flags.IsValid() && outer.isValid() && inner.isValid();
  }
  HAS_SERIALIZATION_FUNCTIONS();

  SkRRect outer;
  SkRRect inner;

 private:
  DrawDRRectOp() = default;
};

class CC_PAINT_EXPORT DrawImageOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawImage;
  static constexpr bool kIsDrawOp = true;
  DrawImageOp(const PaintImage& image,
              SkScalar left,
              SkScalar top,
              const PaintFlags* flags);
  ~DrawImageOp();
  static void RasterWithFlags(const DrawImageOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid(); }
  bool HasDiscardableImages() const;
  bool HasNonAAPaint() const { return false; }
  HAS_SERIALIZATION_FUNCTIONS();

  PaintImage image;
  SkScalar left;
  SkScalar top;

 private:
  DrawImageOp() = default;
};

class CC_PAINT_EXPORT DrawImageRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawImageRect;
  static constexpr bool kIsDrawOp = true;
  DrawImageRectOp(const PaintImage& image,
                  const SkRect& src,
                  const SkRect& dst,
                  const PaintFlags* flags,
                  PaintCanvas::SrcRectConstraint constraint);
  ~DrawImageRectOp();
  static void RasterWithFlags(const DrawImageRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const {
    return flags.IsValid() && src.isFinite() && dst.isFinite();
  }
  bool HasDiscardableImages() const;
  HAS_SERIALIZATION_FUNCTIONS();

  PaintImage image;
  SkRect src;
  SkRect dst;
  PaintCanvas::SrcRectConstraint constraint;

 private:
  DrawImageRectOp();
};

class CC_PAINT_EXPORT DrawIRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawIRect;
  static constexpr bool kIsDrawOp = true;
  DrawIRectOp(const SkIRect& rect, const PaintFlags& flags)
      : PaintOpWithFlags(flags), rect(rect) {}
  static void RasterWithFlags(const DrawIRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid(); }
  bool HasNonAAPaint() const { return false; }
  HAS_SERIALIZATION_FUNCTIONS();

  SkIRect rect;

 private:
  DrawIRectOp() = default;
};

class CC_PAINT_EXPORT DrawLineOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawLine;
  static constexpr bool kIsDrawOp = true;
  DrawLineOp(SkScalar x0,
             SkScalar y0,
             SkScalar x1,
             SkScalar y1,
             const PaintFlags& flags)
      : PaintOpWithFlags(flags), x0(x0), y0(y0), x1(x1), y1(y1) {}
  static void RasterWithFlags(const DrawLineOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid(); }
  HAS_SERIALIZATION_FUNCTIONS();

  int CountSlowPaths() const;

  SkScalar x0;
  SkScalar y0;
  SkScalar x1;
  SkScalar y1;

 private:
  DrawLineOp() = default;
};

class CC_PAINT_EXPORT DrawOvalOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawOval;
  static constexpr bool kIsDrawOp = true;
  DrawOvalOp(const SkRect& oval, const PaintFlags& flags)
      : PaintOpWithFlags(flags), oval(oval) {}
  static void RasterWithFlags(const DrawOvalOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && oval.isFinite(); }
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect oval;

 private:
  DrawOvalOp() = default;
};

class CC_PAINT_EXPORT DrawPathOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawPath;
  static constexpr bool kIsDrawOp = true;
  DrawPathOp(const SkPath& path, const PaintFlags& flags)
      : PaintOpWithFlags(flags), path(path) {}
  static void RasterWithFlags(const DrawPathOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && IsValidPath(path); }
  int CountSlowPaths() const;
  HAS_SERIALIZATION_FUNCTIONS();

  ThreadsafePath path;

 private:
  DrawPathOp() = default;
};

class CC_PAINT_EXPORT DrawRecordOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawRecord;
  static constexpr bool kIsDrawOp = true;
  explicit DrawRecordOp(sk_sp<const PaintRecord> record);
  ~DrawRecordOp();
  static void Raster(const DrawRecordOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  size_t AdditionalBytesUsed() const;
  bool HasDiscardableImages() const;
  int CountSlowPaths() const;
  bool HasNonAAPaint() const;
  HAS_SERIALIZATION_FUNCTIONS();

  sk_sp<const PaintRecord> record;

 private:
  DrawRecordOp();
};

class CC_PAINT_EXPORT DrawRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawRect;
  static constexpr bool kIsDrawOp = true;
  DrawRectOp(const SkRect& rect, const PaintFlags& flags)
      : PaintOpWithFlags(flags), rect(rect) {}
  static void RasterWithFlags(const DrawRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && rect.isFinite(); }
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect rect;

 private:
  DrawRectOp() = default;
};

class CC_PAINT_EXPORT DrawRRectOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawRRect;
  static constexpr bool kIsDrawOp = true;
  DrawRRectOp(const SkRRect& rrect, const PaintFlags& flags)
      : PaintOpWithFlags(flags), rrect(rrect) {}
  static void RasterWithFlags(const DrawRRectOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && rrect.isValid(); }
  HAS_SERIALIZATION_FUNCTIONS();

  SkRRect rrect;

 private:
  DrawRRectOp() = default;
};

class CC_PAINT_EXPORT DrawTextBlobOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawTextBlob;
  static constexpr bool kIsDrawOp = true;
  DrawTextBlobOp(sk_sp<SkTextBlob> blob,
                 SkScalar x,
                 SkScalar y,
                 const PaintFlags& flags);
  ~DrawTextBlobOp();
  static void RasterWithFlags(const DrawTextBlobOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid(); }
  HAS_SERIALIZATION_FUNCTIONS();

  sk_sp<SkTextBlob> blob;
  SkScalar x;
  SkScalar y;

 private:
  DrawTextBlobOp();
};

class CC_PAINT_EXPORT NoopOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Noop;
  static void Raster(const NoopOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params) {}
  bool IsValid() const { return true; }
  HAS_SERIALIZATION_FUNCTIONS();
};

class CC_PAINT_EXPORT RestoreOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Restore;
  static void Raster(const RestoreOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  HAS_SERIALIZATION_FUNCTIONS();
};

class CC_PAINT_EXPORT RotateOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Rotate;
  explicit RotateOp(SkScalar degrees) : degrees(degrees) {}
  static void Raster(const RotateOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  HAS_SERIALIZATION_FUNCTIONS();

  SkScalar degrees;

 private:
  RotateOp() = default;
};

class CC_PAINT_EXPORT SaveOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Save;
  static void Raster(const SaveOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  HAS_SERIALIZATION_FUNCTIONS();
};

class CC_PAINT_EXPORT SaveLayerOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::SaveLayer;
  SaveLayerOp(const SkRect* bounds, const PaintFlags* flags)
      : PaintOpWithFlags(flags ? *flags : PaintFlags()),
        bounds(bounds ? *bounds : kUnsetRect) {}
  static void RasterWithFlags(const SaveLayerOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  bool IsValid() const { return flags.IsValid() && IsValidOrUnsetRect(bounds); }
  bool HasNonAAPaint() const { return false; }
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect bounds;

 private:
  SaveLayerOp() = default;
};

class CC_PAINT_EXPORT SaveLayerAlphaOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::SaveLayerAlpha;
  SaveLayerAlphaOp(const SkRect* bounds,
                   uint8_t alpha,
                   bool preserve_lcd_text_requests)
      : bounds(bounds ? *bounds : kUnsetRect),
        alpha(alpha),
        preserve_lcd_text_requests(preserve_lcd_text_requests) {}
  static void Raster(const SaveLayerAlphaOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return IsValidOrUnsetRect(bounds); }
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect bounds;
  uint8_t alpha;
  bool preserve_lcd_text_requests;

 private:
  SaveLayerAlphaOp() = default;
};

class CC_PAINT_EXPORT ScaleOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Scale;
  ScaleOp(SkScalar sx, SkScalar sy) : sx(sx), sy(sy) {}
  static void Raster(const ScaleOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  HAS_SERIALIZATION_FUNCTIONS();

  SkScalar sx;
  SkScalar sy;

 private:
  ScaleOp() = default;
};

class CC_PAINT_EXPORT SetMatrixOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::SetMatrix;
  explicit SetMatrixOp(const SkMatrix& matrix) : matrix(matrix) {}
  // This is the only op that needs the original ctm of the SkCanvas
  // used for raster (since SetMatrix is relative to the recording origin and
  // shouldn't clobber the SkCanvas raster origin).
  //
  // TODO(enne): Find some cleaner way to do this, possibly by making
  // all SetMatrix calls Concat??
  static void Raster(const SetMatrixOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  HAS_SERIALIZATION_FUNCTIONS();

  ThreadsafeMatrix matrix;

 private:
  SetMatrixOp() = default;
};

class CC_PAINT_EXPORT TranslateOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Translate;
  TranslateOp(SkScalar dx, SkScalar dy) : dx(dx), dy(dy) {}
  static void Raster(const TranslateOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  bool IsValid() const { return true; }
  HAS_SERIALIZATION_FUNCTIONS();

  SkScalar dx;
  SkScalar dy;

 private:
  TranslateOp() = default;
};

#undef HAS_SERIALIZATION_FUNCTIONS

// TODO(vmpstr): Revisit this when sizes of DrawImageRectOp change.
using LargestPaintOp =
    typename std::conditional<(sizeof(DrawImageRectOp) > sizeof(DrawDRRectOp)),
                              DrawImageRectOp,
                              DrawDRRectOp>::type;

class CC_PAINT_EXPORT PaintOpBuffer : public SkRefCnt {
 public:
  enum { kInitialBufferSize = 4096 };
  // It's not necessarily the case that the op with the maximum alignment
  // requirements is also the biggest op, but for now that's true.
  static constexpr size_t PaintOpAlign = alignof(DrawDRRectOp);

  PaintOpBuffer();
  PaintOpBuffer(PaintOpBuffer&& other);
  ~PaintOpBuffer() override;

  void operator=(PaintOpBuffer&& other);

  void Reset();

  // Replays the paint op buffer into the canvas.
  void Playback(SkCanvas* canvas,
                ImageProvider* image_provider = nullptr,
                SkPicture::AbortCallback* callback = nullptr) const;

  // Returns the size of the paint op buffer. That is, the number of ops
  // contained in it.
  size_t size() const { return op_count_; }
  // Returns the number of bytes used by the paint op buffer.
  size_t bytes_used() const {
    return sizeof(*this) + reserved_ + subrecord_bytes_used_;
  }
  size_t next_op_offset() const { return used_; }
  int numSlowPaths() const { return num_slow_paths_; }
  bool HasNonAAPaint() const { return has_non_aa_paint_; }
  bool HasDiscardableImages() const { return has_discardable_images_; }

  // Resize the PaintOpBuffer to exactly fit the current amount of used space.
  void ShrinkToFit();

  const PaintOp* GetFirstOp() const {
    return reinterpret_cast<const PaintOp*>(data_.get());
  }

  template <typename T, typename... Args>
  void push(Args&&... args) {
    static_assert(std::is_convertible<T, PaintOp>::value, "T not a PaintOp.");
    static_assert(alignof(T) <= PaintOpAlign, "");

    auto pair = AllocatePaintOp(sizeof(T));
    T* op = reinterpret_cast<T*>(pair.first);
    size_t skip = pair.second;

    new (op) T{std::forward<Args>(args)...};
    op->type = static_cast<uint32_t>(T::kType);
    op->skip = skip;
    AnalyzeAddedOp(op);
  }

  class CC_PAINT_EXPORT Iterator {
   public:
    explicit Iterator(const PaintOpBuffer* buffer)
        : Iterator(buffer, buffer->data_.get(), 0u) {}

    PaintOp* operator->() const { return reinterpret_cast<PaintOp*>(ptr_); }
    PaintOp* operator*() const { return operator->(); }
    Iterator begin() { return Iterator(buffer_); }
    Iterator end() {
      return Iterator(buffer_, buffer_->data_.get() + buffer_->used_,
                      buffer_->used_);
    }
    bool operator!=(const Iterator& other) {
      // Not valid to compare iterators on different buffers.
      DCHECK_EQ(other.buffer_, buffer_);
      return other.op_offset_ != op_offset_;
    }
    Iterator& operator++() {
      DCHECK(*this);
      const PaintOp* op = **this;
      ptr_ += op->skip;
      op_offset_ += op->skip;

      // Debugging crbug.com/738182.
      base::debug::Alias(op);
      CHECK_LE(op_offset_, buffer_->used_);
      return *this;
    }
    operator bool() const { return op_offset_ < buffer_->used_; }

   private:
    Iterator(const PaintOpBuffer* buffer, char* ptr, size_t op_offset)
        : buffer_(buffer), ptr_(ptr), op_offset_(op_offset) {}

    const PaintOpBuffer* buffer_ = nullptr;
    char* ptr_ = nullptr;
    size_t op_offset_ = 0;
  };

  class CC_PAINT_EXPORT OffsetIterator {
   public:
    // Offsets and paint op buffer must come from the same DisplayItemList.
    OffsetIterator(const PaintOpBuffer* buffer,
                   const std::vector<size_t>* offsets)
        : buffer_(buffer), ptr_(buffer_->data_.get()), offsets_(offsets) {
      if (offsets->empty()) {
        *this = end();
        return;
      }
      op_offset_ = (*offsets)[0];
      ptr_ += op_offset_;
    }

    PaintOp* operator->() const { return reinterpret_cast<PaintOp*>(ptr_); }
    PaintOp* operator*() const { return operator->(); }
    OffsetIterator begin() { return OffsetIterator(buffer_, offsets_); }
    OffsetIterator end() {
      return OffsetIterator(buffer_, buffer_->data_.get() + buffer_->used_,
                            buffer_->used_, offsets_);
    }
    bool operator!=(const OffsetIterator& other) {
      // Not valid to compare iterators on different buffers.
      DCHECK_EQ(other.buffer_, buffer_);
      return other.op_offset_ != op_offset_;
    }
    OffsetIterator& operator++() {
      if (++offsets_index_ >= offsets_->size()) {
        *this = end();
        return *this;
      }

      size_t target_offset = (*offsets_)[offsets_index_];
      // Sanity checks.
      CHECK_GE(target_offset, op_offset_);
      // Debugging crbug.com/738182.
      base::debug::Alias(&target_offset);
      CHECK_LT(target_offset, buffer_->used_);

      // Advance the iterator to the target offset.
      ptr_ += (target_offset - op_offset_);
      op_offset_ = target_offset;

      DCHECK(!*this || (*this)->type <=
                           static_cast<uint32_t>(PaintOpType::LastPaintOpType));
      return *this;
    }

    operator bool() const { return op_offset_ < buffer_->used_; }

   private:
    OffsetIterator(const PaintOpBuffer* buffer,
                   char* ptr,
                   size_t op_offset,
                   const std::vector<size_t>* offsets)
        : buffer_(buffer),
          ptr_(ptr),
          offsets_(offsets),
          op_offset_(op_offset) {}

    const PaintOpBuffer* buffer_ = nullptr;
    char* ptr_ = nullptr;
    const std::vector<size_t>* offsets_;
    size_t op_offset_ = 0;
    size_t offsets_index_ = 0;
  };

  class CC_PAINT_EXPORT CompositeIterator {
   public:
    // Offsets and paint op buffer must come from the same DisplayItemList.
    CompositeIterator(const PaintOpBuffer* buffer,
                      const std::vector<size_t>* offsets);
    CompositeIterator(const CompositeIterator& other);
    CompositeIterator(CompositeIterator&& other);

    PaintOp* operator->() const {
      return using_offsets_ ? **offset_iter_ : **iter_;
    }
    PaintOp* operator*() const {
      return using_offsets_ ? **offset_iter_ : **iter_;
    }
    bool operator==(const CompositeIterator& other) {
      if (using_offsets_ != other.using_offsets_)
        return false;
      return using_offsets_ ? (*offset_iter_ == *other.offset_iter_)
                            : (*iter_ == *other.iter_);
    }
    bool operator!=(const CompositeIterator& other) {
      return !(*this == other);
    }
    CompositeIterator& operator++() {
      if (using_offsets_)
        ++*offset_iter_;
      else
        ++*iter_;
      return *this;
    }
    operator bool() const {
      return using_offsets_ ? !!*offset_iter_ : !!*iter_;
    }

   private:
    bool using_offsets_ = false;
    base::Optional<OffsetIterator> offset_iter_;
    base::Optional<Iterator> iter_;
  };

  // Returns a stream of non-DrawRecord ops from a top level pob with indices.
  // Upon encountering DrawRecord ops, it returns ops from inside them
  // without returning the DrawRecord op itself.  It does this recursively.
  class CC_PAINT_EXPORT FlatteningIterator {
   public:
    // Offsets and paint op buffer must come from the same DisplayItemList.
    FlatteningIterator(const PaintOpBuffer* buffer,
                       const std::vector<size_t>* offsets);
    ~FlatteningIterator();

    PaintOp* operator->() const {
      return nested_iter_.empty() ? *top_level_iter_ : *nested_iter_.back();
    }
    PaintOp* operator*() const { return operator->(); }

    FlatteningIterator& operator++() {
      if (nested_iter_.empty())
        ++top_level_iter_;
      else
        ++nested_iter_.back();
      FlattenCurrentOpIfNeeded();
      return *this;
    }

    operator bool() const { return top_level_iter_; }

   private:
    void FlattenCurrentOpIfNeeded();

    PaintOpBuffer::OffsetIterator top_level_iter_;
    std::vector<Iterator> nested_iter_;
  };

 private:
  friend class DisplayItemList;
  friend class PaintOpBufferOffsetsTest;
  friend class SolidColorAnalyzer;
  friend class ScopedImageFlags;

  // Replays the paint op buffer into the canvas. If |indices| is specified, it
  // contains indices in an increasing order and only the indices specified in
  // the vector will be replayed.
  void Playback(SkCanvas* canvas,
                ImageProvider* image_provider,
                SkPicture::AbortCallback* callback,
                const std::vector<size_t>* indices) const;

  void ReallocBuffer(size_t new_size);
  // Returns the allocated op and the number of bytes to skip in |data_| to get
  // to the next op.
  std::pair<void*, size_t> AllocatePaintOp(size_t sizeof_op);

  template <typename T>
  void AnalyzeAddedOp(const T* op) {
    num_slow_paths_ += op->CountSlowPathsFromFlags();
    num_slow_paths_ += op->CountSlowPaths();

    has_non_aa_paint_ |= op->HasNonAAPaint();

    has_discardable_images_ |= op->HasDiscardableImages();
    has_discardable_images_ |= op->HasDiscardableImagesFromFlags();

    subrecord_bytes_used_ += op->AdditionalBytesUsed();
  }

  std::unique_ptr<char, base::AlignedFreeDeleter> data_;
  size_t used_ = 0;
  size_t reserved_ = 0;
  size_t op_count_ = 0;

  // Record paths for veto-to-msaa for gpu raster.
  int num_slow_paths_ = 0;
  // Record additional bytes used by referenced sub-records and display lists.
  size_t subrecord_bytes_used_ = 0;

  bool has_non_aa_paint_ : 1;
  bool has_discardable_images_ : 1;

  DISALLOW_COPY_AND_ASSIGN(PaintOpBuffer);
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_BUFFER_H_
