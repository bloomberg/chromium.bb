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
class ImageProvider;

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
#define HAS_SERIALIZATION_FUNCTIONS()                                   \
  static size_t Serialize(const PaintOp* op, void* memory, size_t size, \
                          const SerializeOptions& options);             \
  static PaintOp* Deserialize(const void* input, size_t input_size,     \
                              void* output, size_t output_size);

enum class PaintOpType : uint8_t {
  Annotate,
  ClipPath,
  ClipRect,
  ClipRRect,
  Concat,
  DrawArc,
  DrawCircle,
  DrawColor,
  DrawDRRect,
  DrawImage,
  DrawImageRect,
  DrawIRect,
  DrawLine,
  DrawOval,
  DrawPath,
  DrawPosText,
  DrawRecord,
  DrawRect,
  DrawRRect,
  DrawText,
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
  static PaintOp* Deserialize(const void* input,
                              size_t input_size,
                              void* output,
                              size_t output_size);

  // For draw ops, returns true if a conservative bounding rect can be provided
  // for the op.
  static bool GetBounds(const PaintOp* op, SkRect* rect);

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
  bool HasDiscardableImagesFromFlags() const {
    if (!IsDrawOp())
      return false;

    SkShader* shader = flags.getSkShader();
    SkImage* image = shader ? shader->isAImage(nullptr, nullptr) : nullptr;
    return image && image->isLazyGenerated();
  }

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

class CC_PAINT_EXPORT PaintOpWithData : public PaintOpWithFlags {
 public:
  // Having data is just a helper for ops that have a varying amount of data and
  // want a way to store that inline.  This is for ops that pass in a
  // void* and a length.  The void* data is assumed to not have any alignment
  // requirements.
  PaintOpWithData(const PaintFlags& flags, size_t bytes)
      : PaintOpWithFlags(flags), bytes(bytes) {}

  // Get data out by calling paint_op_data.  This can't be part of the class
  // because it needs to know the size of the derived type.
  size_t bytes;

 protected:
  PaintOpWithData() = default;

  // For some derived object T, return the internally stored data.
  // This needs the fully derived type to know how much to offset
  // from the start of the top to the data.
  template <typename T>
  const void* GetDataForThis(const T* op) const {
    static_assert(std::is_convertible<T, PaintOpWithData>::value,
                  "T is not a PaintOpWithData");
    // Arbitrary data for a PaintOp is stored after the PaintOp itself
    // in the PaintOpBuffer.  Therefore, to access this data, it's
    // pointer math to increment past the size of T.  Accessing the
    // next op in the buffer is ((char*)op) + op->skip, with the data
    // fitting between.
    return op + 1;
  }

  template <typename T>
  void* GetDataForThis(T* op) {
    return const_cast<void*>(
        const_cast<const PaintOpWithData*>(this)->GetDataForThis(
            const_cast<const T*>(op)));
  }
};

class CC_PAINT_EXPORT PaintOpWithArrayBase : public PaintOpWithFlags {
 public:
  explicit PaintOpWithArrayBase(const PaintFlags& flags)
      : PaintOpWithFlags(flags) {}

 protected:
  PaintOpWithArrayBase() = default;
};

template <typename M>
class CC_PAINT_EXPORT PaintOpWithArray : public PaintOpWithArrayBase {
 public:
  // Paint op that has a M[count] and a char[bytes].
  // Array data is stored first so that it can be aligned with T's alignment
  // with the arbitrary unaligned char data after it.
  // Memory layout here is: | op | M[count] | char[bytes] | padding | next op |
  // Next op is located at (char*)(op) + op->skip.
  PaintOpWithArray(const PaintFlags& flags, size_t bytes, size_t count)
      : PaintOpWithArrayBase(flags), bytes(bytes), count(count) {}

  size_t bytes;
  size_t count;

 protected:
  PaintOpWithArray() = default;

  template <typename T>
  const void* GetDataForThis(const T* op) const {
    static_assert(std::is_convertible<T, PaintOpWithArrayBase>::value,
                  "T is not a PaintOpWithData");
    const char* start_array =
        reinterpret_cast<const char*>(GetArrayForThis(op));
    return start_array + sizeof(M) * count;
  }

  template <typename T>
  void* GetDataForThis(T* op) {
    return const_cast<void*>(
        const_cast<const PaintOpWithArray*>(this)->GetDataForThis(
            const_cast<T*>(op)));
  }

  template <typename T>
  const M* GetArrayForThis(const T* op) const {
    static_assert(std::is_convertible<T, PaintOpWithArrayBase>::value,
                  "T is not a PaintOpWithData");
    // As an optimization to not have to store an additional offset,
    // assert that T has the same or more alignment requirements than M.  Thus,
    // if T is aligned, and M's alignment needs are a multiple of T's size, then
    // M will also be aligned when placed immediately after T.
    static_assert(
        sizeof(T) % alignof(M) == 0,
        "T must be padded such that an array of M is aligned after it");
    static_assert(
        alignof(T) >= alignof(M),
        "T must have not have less alignment requirements than the array data");
    return reinterpret_cast<const M*>(op + 1);
  }

  template <typename T>
  M* GetArrayForThis(T* op) {
    return const_cast<M*>(
        const_cast<const PaintOpWithArray*>(this)->GetArrayForThis(
            const_cast<T*>(op)));
  }
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
  HAS_SERIALIZATION_FUNCTIONS();

  ThreadsafeMatrix matrix;

 private:
  ConcatOp() = default;
};

class CC_PAINT_EXPORT DrawArcOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawArc;
  static constexpr bool kIsDrawOp = true;
  DrawArcOp(const SkRect& oval,
            SkScalar start_angle,
            SkScalar sweep_angle,
            bool use_center,
            const PaintFlags& flags)
      : PaintOpWithFlags(flags),
        oval(oval),
        start_angle(start_angle),
        sweep_angle(sweep_angle),
        use_center(use_center) {}
  static void RasterWithFlags(const DrawArcOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  HAS_SERIALIZATION_FUNCTIONS();

  SkRect oval;
  SkScalar start_angle;
  SkScalar sweep_angle;
  bool use_center;

 private:
  DrawArcOp() = default;
};

class CC_PAINT_EXPORT DrawCircleOp final : public PaintOpWithFlags {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawCircle;
  static constexpr bool kIsDrawOp = true;
  DrawCircleOp(SkScalar cx,
               SkScalar cy,
               SkScalar radius,
               const PaintFlags& flags)
      : PaintOpWithFlags(flags), cx(cx), cy(cy), radius(radius) {}
  static void RasterWithFlags(const DrawCircleOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  HAS_SERIALIZATION_FUNCTIONS();

  SkScalar cx;
  SkScalar cy;
  SkScalar radius;

 private:
  DrawCircleOp() = default;
};

class CC_PAINT_EXPORT DrawColorOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawColor;
  static constexpr bool kIsDrawOp = true;
  DrawColorOp(SkColor color, SkBlendMode mode) : color(color), mode(mode) {}
  static void Raster(const DrawColorOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
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
  int CountSlowPaths() const;
  HAS_SERIALIZATION_FUNCTIONS();

  ThreadsafePath path;

 private:
  DrawPathOp() = default;
};

class CC_PAINT_EXPORT DrawPosTextOp final : public PaintOpWithArray<SkPoint> {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawPosText;
  static constexpr bool kIsDrawOp = true;
  DrawPosTextOp(size_t bytes, size_t count, const PaintFlags& flags);
  ~DrawPosTextOp();
  static void RasterWithFlags(const DrawPosTextOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  HAS_SERIALIZATION_FUNCTIONS();

  const void* GetData() const { return GetDataForThis(this); }
  void* GetData() { return GetDataForThis(this); }
  const SkPoint* GetArray() const { return GetArrayForThis(this); }
  SkPoint* GetArray() { return GetArrayForThis(this); }

 private:
  DrawPosTextOp();
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
  HAS_SERIALIZATION_FUNCTIONS();

  SkRRect rrect;

 private:
  DrawRRectOp() = default;
};

class CC_PAINT_EXPORT DrawTextOp final : public PaintOpWithData {
 public:
  static constexpr PaintOpType kType = PaintOpType::DrawText;
  static constexpr bool kIsDrawOp = true;
  DrawTextOp(size_t bytes, SkScalar x, SkScalar y, const PaintFlags& flags)
      : PaintOpWithData(flags, bytes), x(x), y(y) {}
  static void RasterWithFlags(const DrawTextOp* op,
                              const PaintFlags* flags,
                              SkCanvas* canvas,
                              const PlaybackParams& params);
  HAS_SERIALIZATION_FUNCTIONS();

  void* GetData() { return GetDataForThis(this); }
  const void* GetData() const { return GetDataForThis(this); }

  SkScalar x;
  SkScalar y;

 private:
  DrawTextOp() = default;
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
  HAS_SERIALIZATION_FUNCTIONS();
};

class CC_PAINT_EXPORT RestoreOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Restore;
  static void Raster(const RestoreOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
  HAS_SERIALIZATION_FUNCTIONS();
};

class CC_PAINT_EXPORT RotateOp final : public PaintOp {
 public:
  static constexpr PaintOpType kType = PaintOpType::Rotate;
  explicit RotateOp(SkScalar degrees) : degrees(degrees) {}
  static void Raster(const RotateOp* op,
                     SkCanvas* canvas,
                     const PlaybackParams& params);
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
    static_assert(!std::is_convertible<T, PaintOpWithData>::value,
                  "Type needs to use push_with_data");
    push_internal<T>(0, std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  void push_with_data(const void* data, size_t bytes, Args&&... args) {
    static_assert(std::is_convertible<T, PaintOpWithData>::value,
                  "T is not a PaintOpWithData");
    static_assert(!std::is_convertible<T, PaintOpWithArrayBase>::value,
                  "Type needs to use push_with_array");
    T* op = push_internal<T>(bytes, bytes, std::forward<Args>(args)...);
    memcpy(op->GetData(), data, bytes);

#if DCHECK_IS_ON()
    // Double check the data fits between op and next op and doesn't clobber.
    char* op_start = reinterpret_cast<char*>(op);
    char* op_end = op_start + sizeof(T);
    char* next_op = op_start + op->skip;
    char* data_start = reinterpret_cast<char*>(op->GetData());
    char* data_end = data_start + bytes;
    DCHECK_GE(data_start, op_end);
    DCHECK_LT(data_start, next_op);
    DCHECK_LE(data_end, next_op);
#endif
  }

  template <typename T, typename M, typename... Args>
  void push_with_array(const void* data,
                       size_t bytes,
                       const M* array,
                       size_t count,
                       Args&&... args) {
    static_assert(std::is_convertible<T, PaintOpWithArray<M>>::value,
                  "T is not a PaintOpWithArray");
    size_t array_size = sizeof(M) * count;
    size_t total_size = bytes + array_size;
    T* op =
        push_internal<T>(total_size, bytes, count, std::forward<Args>(args)...);
    memcpy(op->GetData(), data, bytes);
    memcpy(op->GetArray(), array, array_size);

#if DCHECK_IS_ON()
    // Double check data and array don't clobber op, next op, or each other
    char* op_start = reinterpret_cast<char*>(op);
    char* op_end = op_start + sizeof(T);
    char* array_start = reinterpret_cast<char*>(op->GetArray());
    char* array_end = array_start + array_size;
    char* data_start = reinterpret_cast<char*>(op->GetData());
    char* data_end = data_start + bytes;
    char* next_op = op_start + op->skip;
    DCHECK_GE(array_start, op_end);
    DCHECK_LE(array_start, data_start);
    DCHECK_GE(data_start, array_end);
    DCHECK_LE(data_end, next_op);
#endif
  }

  class Iterator {
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

 private:
  friend class DisplayItemList;
  friend class PaintOpBufferOffsetsTest;
  friend class SolidColorAnalyzer;

  class OffsetIterator {
   public:
    // We only trust with the offsets from the friend classes.
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

  class CompositeIterator {
   public:
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
  std::pair<void*, size_t> AllocatePaintOp(size_t sizeof_op, size_t bytes);

  template <typename T, typename... Args>
  T* push_internal(size_t bytes, Args&&... args) {
    static_assert(alignof(T) <= PaintOpAlign, "");

    auto pair = AllocatePaintOp(sizeof(T), bytes);
    T* op = reinterpret_cast<T*>(pair.first);
    size_t skip = pair.second;

    new (op) T{std::forward<Args>(args)...};
    op->type = static_cast<uint32_t>(T::kType);
    op->skip = skip;
    AnalyzeAddedOp(op);
    return op;
  }

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
