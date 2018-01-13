// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PAINT_OP_HELPER_H_
#define CC_TEST_PAINT_OP_HELPER_H_

#include <sstream>
#include <string>

#include "base/strings/stringprintf.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op_buffer.h"

namespace cc {

// A helper class to help with debugging PaintOp/PaintOpBuffer.
// Note that this file is primarily used for debugging. As such, it isn't
// typically a part of BUILD.gn (except for self-testing), so all of the
// implementation should be limited ot the header.
class PaintOpHelper {
 public:
  static std::string ToString(const PaintOp* base_op) {
    std::ostringstream str;
    str << std::boolalpha;
    switch (base_op->GetType()) {
      case PaintOpType::Annotate: {
        const auto* op = static_cast<const AnnotateOp*>(base_op);
        str << "AnnotateOp(type="
            << PaintOpHelper::EnumToString(op->annotation_type)
            << ", rect=" << PaintOpHelper::SkiaTypeToString(op->rect)
            << ", data=" << PaintOpHelper::SkiaTypeToString(op->data) << ")";
        break;
      }
      case PaintOpType::ClipPath: {
        const auto* op = static_cast<const ClipPathOp*>(base_op);
        str << "ClipPathOp(path=" << PaintOpHelper::SkiaTypeToString(op->path)
            << ", op=" << PaintOpHelper::SkiaTypeToString(op->op)
            << ", antialias=" << op->antialias << ")";
        break;
      }
      case PaintOpType::ClipRect: {
        const auto* op = static_cast<const ClipRectOp*>(base_op);
        str << "ClipRectOp(rect=" << PaintOpHelper::SkiaTypeToString(op->rect)
            << ", op=" << PaintOpHelper::SkiaTypeToString(op->op)
            << ", antialias=" << op->antialias << ")";
        break;
      }
      case PaintOpType::ClipRRect: {
        const auto* op = static_cast<const ClipRRectOp*>(base_op);
        str << "ClipRRectOp(rrect="
            << PaintOpHelper::SkiaTypeToString(op->rrect)
            << ", op=" << PaintOpHelper::SkiaTypeToString(op->op)
            << ", antialias=" << op->antialias << ")";
        break;
      }
      case PaintOpType::Concat: {
        const auto* op = static_cast<const ConcatOp*>(base_op);
        str << "ConcatOp(matrix=" << PaintOpHelper::SkiaTypeToString(op->matrix)
            << ")";
        break;
      }
      case PaintOpType::CustomData: {
        const auto* op = static_cast<const CustomDataOp*>(base_op);
        str << "CustomDataOp(id=" << PaintOpHelper::SkiaTypeToString(op->id)
            << ")";
        break;
      }
      case PaintOpType::DrawColor: {
        const auto* op = static_cast<const DrawColorOp*>(base_op);
        str << "DrawColorOp(color="
            << PaintOpHelper::SkiaTypeToString(op->color)
            << ", mode=" << PaintOpHelper::SkiaTypeToString(op->mode) << ")";
        break;
      }
      case PaintOpType::DrawDRRect: {
        const auto* op = static_cast<const DrawDRRectOp*>(base_op);
        str << "DrawDRRectOp(outer="
            << PaintOpHelper::SkiaTypeToString(op->outer)
            << ", inner=" << PaintOpHelper::SkiaTypeToString(op->inner)
            << ", flags=" << PaintOpHelper::FlagsToString(op->flags) << ")";
        break;
      }
      case PaintOpType::DrawImage: {
        const auto* op = static_cast<const DrawImageOp*>(base_op);
        str << "DrawImageOp(image=" << PaintOpHelper::ImageToString(op->image)
            << ", left=" << PaintOpHelper::SkiaTypeToString(op->left)
            << ", top=" << PaintOpHelper::SkiaTypeToString(op->top)
            << ", flags=" << PaintOpHelper::FlagsToString(op->flags) << ")";
        break;
      }
      case PaintOpType::DrawImageRect: {
        const auto* op = static_cast<const DrawImageRectOp*>(base_op);
        str << "DrawImageRectOp(image="
            << PaintOpHelper::ImageToString(op->image)
            << ", src=" << PaintOpHelper::SkiaTypeToString(op->src)
            << ", dst=" << PaintOpHelper::SkiaTypeToString(op->dst)
            << ", constraint=" << PaintOpHelper::EnumToString(op->constraint)
            << ", flags=" << PaintOpHelper::FlagsToString(op->flags) << ")";
        break;
      }
      case PaintOpType::DrawIRect: {
        const auto* op = static_cast<const DrawIRectOp*>(base_op);
        str << "DrawIRectOp(rect=" << PaintOpHelper::SkiaTypeToString(op->rect)
            << ", flags=" << PaintOpHelper::FlagsToString(op->flags) << ")";
        break;
      }
      case PaintOpType::DrawLine: {
        const auto* op = static_cast<const DrawLineOp*>(base_op);
        str << "DrawLineOp(x0=" << PaintOpHelper::SkiaTypeToString(op->x0)
            << ", y0=" << PaintOpHelper::SkiaTypeToString(op->y0)
            << ", x1=" << PaintOpHelper::SkiaTypeToString(op->x1)
            << ", y1=" << PaintOpHelper::SkiaTypeToString(op->y1)
            << ", flags=" << PaintOpHelper::FlagsToString(op->flags) << ")";
        break;
      }
      case PaintOpType::DrawOval: {
        const auto* op = static_cast<const DrawOvalOp*>(base_op);
        str << "DrawOvalOp(oval=" << PaintOpHelper::SkiaTypeToString(op->oval)
            << ", flags=" << PaintOpHelper::FlagsToString(op->flags) << ")";
        break;
      }
      case PaintOpType::DrawPath: {
        const auto* op = static_cast<const DrawPathOp*>(base_op);
        str << "DrawPathOp(path=" << PaintOpHelper::SkiaTypeToString(op->path)
            << ", flags=" << PaintOpHelper::FlagsToString(op->flags) << ")";
        break;
      }
      case PaintOpType::DrawRecord: {
        const auto* op = static_cast<const DrawRecordOp*>(base_op);
        str << "DrawRecordOp(record="
            << PaintOpHelper::RecordToString(op->record) << ")";
        break;
      }
      case PaintOpType::DrawRect: {
        const auto* op = static_cast<const DrawRectOp*>(base_op);
        str << "DrawRectOp(rect=" << PaintOpHelper::SkiaTypeToString(op->rect)
            << ", flags=" << PaintOpHelper::FlagsToString(op->flags) << ")";
        break;
      }
      case PaintOpType::DrawRRect: {
        const auto* op = static_cast<const DrawRRectOp*>(base_op);
        str << "DrawRRectOp(rrect="
            << PaintOpHelper::SkiaTypeToString(op->rrect)
            << ", flags=" << PaintOpHelper::FlagsToString(op->flags) << ")";
        break;
      }
      case PaintOpType::DrawTextBlob: {
        const auto* op = static_cast<const DrawTextBlobOp*>(base_op);
        str << "DrawTextBlobOp(blob="
            << PaintOpHelper::TextBlobToString(op->blob)
            << ", x=" << PaintOpHelper::SkiaTypeToString(op->x)
            << ", y=" << PaintOpHelper::SkiaTypeToString(op->y)
            << ", flags=" << PaintOpHelper::FlagsToString(op->flags) << ")";
        break;
      }
      case PaintOpType::Noop: {
        str << "NoopOp()";
        break;
      }
      case PaintOpType::Restore: {
        str << "RestoreOp()";
        break;
      }
      case PaintOpType::Rotate: {
        const auto* op = static_cast<const RotateOp*>(base_op);
        str << "RotateOp(degrees="
            << PaintOpHelper::SkiaTypeToString(op->degrees) << ")";
        break;
      }
      case PaintOpType::Save: {
        str << "SaveOp()";
        break;
      }
      case PaintOpType::SaveLayer: {
        const auto* op = static_cast<const SaveLayerOp*>(base_op);
        str << "SaveLayerOp(bounds="
            << PaintOpHelper::SkiaTypeToString(op->bounds)
            << ", flags=" << PaintOpHelper::FlagsToString(op->flags) << ")";
        break;
      }
      case PaintOpType::SaveLayerAlpha: {
        const auto* op = static_cast<const SaveLayerAlphaOp*>(base_op);
        str << "SaveLayerAlphaOp(bounds="
            << PaintOpHelper::SkiaTypeToString(op->bounds)
            << ", alpha=" << static_cast<uint32_t>(op->alpha)
            << ", preserve_lcd_text_requests=" << op->preserve_lcd_text_requests
            << ")";
        break;
      }
      case PaintOpType::Scale: {
        const auto* op = static_cast<const ScaleOp*>(base_op);
        str << "ScaleOp(sx=" << PaintOpHelper::SkiaTypeToString(op->sx)
            << ", sy=" << PaintOpHelper::SkiaTypeToString(op->sy) << ")";
        break;
      }
      case PaintOpType::SetMatrix: {
        const auto* op = static_cast<const SetMatrixOp*>(base_op);
        str << "SetMatrixOp(matrix="
            << PaintOpHelper::SkiaTypeToString(op->matrix) << ")";
        break;
      }
      case PaintOpType::Translate: {
        const auto* op = static_cast<const TranslateOp*>(base_op);
        str << "TranslateOp(dx=" << PaintOpHelper::SkiaTypeToString(op->dx)
            << ", dy=" << PaintOpHelper::SkiaTypeToString(op->dy) << ")";
        break;
      }
    }
    return str.str();
  }

  template <typename T>
  static std::string SkiaTypeToString(const T&) {
    return "<unknown skia type>";
  }

  static std::string SkiaTypeToString(const SkScalar& scalar) {
    return base::StringPrintf("%.3f", scalar);
  }

  static std::string SkiaTypeToString(const SkRect& rect) {
    return base::StringPrintf("[%.3f,%.3f %.3fx%.3f]", rect.x(), rect.y(),
                              rect.width(), rect.height());
  }

  static std::string SkiaTypeToString(const SkIRect& rect) {
    return base::StringPrintf("[%d,%d %dx%d]", rect.x(), rect.y(), rect.width(),
                              rect.height());
  }

  static std::string SkiaTypeToString(const SkRRect& rect) {
    return base::StringPrintf("[bounded by %.3f,%.3f %.3fx%.3f]",
                              rect.rect().x(), rect.rect().y(),
                              rect.rect().width(), rect.rect().height());
  }

  static std::string SkiaTypeToString(const ThreadsafeMatrix& matrix) {
    return SkiaTypeToString(static_cast<const SkMatrix&>(matrix));
  }

  static std::string SkiaTypeToString(const SkMatrix& matrix) {
    SkString str;
    matrix.toString(&str);
    return str.c_str();
  }

  static std::string SkiaTypeToString(const SkColor& color) {
    return base::StringPrintf("rgba(%d, %d, %d, %d)", SkColorGetR(color),
                              SkColorGetG(color), SkColorGetB(color),
                              SkColorGetA(color));
  }

  static std::string SkiaTypeToString(const SkBlendMode& mode) {
    switch (mode) {
      default:
        break;
      case SkBlendMode::kClear:
        return "kClear";
      case SkBlendMode::kSrc:
        return "kSrc";
      case SkBlendMode::kDst:
        return "kDst";
      case SkBlendMode::kSrcOver:
        return "kSrcOver";
      case SkBlendMode::kDstOver:
        return "kDstOver";
      case SkBlendMode::kSrcIn:
        return "kSrcIn";
      case SkBlendMode::kDstIn:
        return "kDstIn";
      case SkBlendMode::kSrcOut:
        return "kSrcOut";
      case SkBlendMode::kDstOut:
        return "kDstOut";
      case SkBlendMode::kSrcATop:
        return "kSrcATop";
      case SkBlendMode::kDstATop:
        return "kDstATop";
      case SkBlendMode::kXor:
        return "kXor";
      case SkBlendMode::kPlus:
        return "kPlus";
      case SkBlendMode::kModulate:
        return "kModulate";
      case SkBlendMode::kScreen:
        return "kScreen";
      case SkBlendMode::kOverlay:
        return "kOverlay";
      case SkBlendMode::kDarken:
        return "kDarken";
      case SkBlendMode::kLighten:
        return "kLighten";
      case SkBlendMode::kColorDodge:
        return "kColorDodge";
      case SkBlendMode::kColorBurn:
        return "kColorBurn";
      case SkBlendMode::kHardLight:
        return "kHardLight";
      case SkBlendMode::kSoftLight:
        return "kSoftLight";
      case SkBlendMode::kDifference:
        return "kDifference";
      case SkBlendMode::kExclusion:
        return "kExclusion";
      case SkBlendMode::kMultiply:
        return "kMultiply";
      case SkBlendMode::kHue:
        return "kHue";
      case SkBlendMode::kSaturation:
        return "kSaturation";
      case SkBlendMode::kColor:
        return "kColor";
      case SkBlendMode::kLuminosity:
        return "kLuminosity";
    }
    return "<unknown SkBlendMode>";
  }

  static std::string SkiaTypeToString(const SkClipOp& op) {
    switch (op) {
      default:
        break;
      case SkClipOp::kDifference:
        return "kDifference";
      case SkClipOp::kIntersect:
        return "kIntersect";
    }
    return "<unknown SkClipOp>";
  }

  static std::string SkiaTypeToString(const sk_sp<SkData> data) {
    return data ? "<SkData>" : "(nil)";
  }

  static std::string SkiaTypeToString(const ThreadsafePath& path) {
    return SkiaTypeToString(static_cast<const SkPath&>(path));
  }

  static std::string SkiaTypeToString(const SkPath& path) {
    // TODO(vmpstr): SkPath has a dump function which we can use here?
    return "<SkPath>";
  }

  static std::string SkiaTypeToString(PaintFlags::Hinting hinting) {
    switch (hinting) {
      case PaintFlags::kNo_Hinting:
        return "kNo_Hinting";
      case PaintFlags::kSlight_Hinting:
        return "kSlight_Hinting";
      case PaintFlags::kNormal_Hinting:
        return "kNormal_Hinting";
      case PaintFlags::kFull_Hinting:
        return "kFull_Hinting";
    }
    return "<unknown PaintFlags::Hinting>";
  }

  static std::string SkiaTypeToString(PaintFlags::TextEncoding encoding) {
    switch (encoding) {
      case PaintFlags::kUTF8_TextEncoding:
        return "kUTF8_TextEncoding";
      case PaintFlags::kUTF16_TextEncoding:
        return "kUTF16_TextEncoding";
      case PaintFlags::kUTF32_TextEncoding:
        return "kUTF32_TextEncoding";
      case PaintFlags::kGlyphID_TextEncoding:
        return "kGlyphID_TextEncoding";
    }
    return "<unknown PaintFlags::TextEncoding>";
  }

  static std::string SkiaTypeToString(SkFilterQuality quality) {
    switch (quality) {
      case kNone_SkFilterQuality:
        return "kNone_SkFilterQuality";
      case kLow_SkFilterQuality:
        return "kLow_SkFilterQuality";
      case kMedium_SkFilterQuality:
        return "kMedium_SkFilterQuality";
      case kHigh_SkFilterQuality:
        return "kHigh_SkFilterQuality";
    }
    return "<unknown SkFilterQuality>";
  }

  static std::string SkiaTypeToString(PaintFlags::Cap cap) {
    switch (cap) {
      case PaintFlags::kButt_Cap:
        return "kButt_Cap";
      case PaintFlags::kRound_Cap:
        return "kRound_Cap";
      case PaintFlags::kSquare_Cap:
        return "kSquare_Cap";
    }
    return "<unknown PaintFlags::Cap>";
  }

  static std::string SkiaTypeToString(PaintFlags::Join join) {
    switch (join) {
      case PaintFlags::kMiter_Join:
        return "kMiter_Join";
      case PaintFlags::kRound_Join:
        return "kRound_Join";
      case PaintFlags::kBevel_Join:
        return "kBevel_Join";
    }
    return "<unknown PaintFlags::Join>";
  }

  static std::string SkiaTypeToString(const sk_sp<SkTypeface>& typeface) {
    return typeface ? "<SkTypeface>" : "(nil)";
  }

  static std::string SkiaTypeToString(const sk_sp<SkColorFilter>& filter) {
    if (!filter)
      return "(nil)";
    SkString str;
    filter->toString(&str);
    return str.c_str();
  }

  static std::string SkiaTypeToString(const sk_sp<SkMaskFilter>& filter) {
    if (!filter)
      return "(nil)";
    SkString str;
    filter->toString(&str);
    return str.c_str();
  }

  static std::string SkiaTypeToString(const sk_sp<SkPathEffect>& effect) {
    if (!effect)
      return "(nil)";
    SkString str;
    effect->toString(&str);
    return str.c_str();
  }

  static std::string SkiaTypeToString(const sk_sp<SkDrawLooper>& looper) {
    if (!looper)
      return "(nil)";
    SkString str;
    looper->toString(&str);
    return str.c_str();
  }

  template <typename T>
  static std::string EnumToString(T) {
    return "<unknown enum type>";
  }

  static std::string EnumToString(PaintCanvas::AnnotationType type) {
    switch (type) {
      default:
        break;
      case PaintCanvas::AnnotationType::URL:
        return "URL";
      case PaintCanvas::AnnotationType::NAMED_DESTINATION:
        return "NAMED_DESTINATION";
      case PaintCanvas::AnnotationType::LINK_TO_DESTINATION:
        return "LINK_TO_DESTINATION";
    }
    return "<unknown AnnotationType>";
  }

  static std::string EnumToString(
      const PaintCanvas::SrcRectConstraint& constraint) {
    switch (constraint) {
      default:
        break;
      case PaintCanvas::kStrict_SrcRectConstraint:
        return "kStrict_SrcRectConstraint";
      case PaintCanvas::kFast_SrcRectConstraint:
        return "kFast_SrcRectConstraint";
    }
    return "<unknown SrcRectConstraint>";
  }

  static std::string ImageToString(const PaintImage& image) {
    return "<paint image>";
  }

  static std::string RecordToString(const sk_sp<const PaintRecord>& record) {
    return record ? "<paint record>" : "(nil)";
  }

  static std::string TextBlobToString(
      const scoped_refptr<PaintTextBlob>& blob) {
    return blob ? "<paint text blob>" : "(nil)";
  }

  static std::string PaintShaderToString(const PaintShader* shader) {
    return shader ? "<PaintShader>" : "(nil)";
  }

  static std::string PaintFilterToString(const sk_sp<PaintFilter> filter) {
    return filter ? "<PaintFilter>" : "(nil)";
  }

  static std::string FlagsToString(const PaintFlags& flags) {
    std::ostringstream str;
    str << std::boolalpha;
    str << "[color=" << PaintOpHelper::SkiaTypeToString(flags.getColor());
    str << ", blendMode="
        << PaintOpHelper::SkiaTypeToString(flags.getBlendMode());
    str << ", isAntiAlias=" << flags.isAntiAlias();
    str << ", isVerticalText=" << flags.isVerticalText();
    str << ", isSubpixelText=" << flags.isSubpixelText();
    str << ", isLCDRenderText=" << flags.isLCDRenderText();
    str << ", hinting=" << PaintOpHelper::SkiaTypeToString(flags.getHinting());
    str << ", isAutohinted=" << flags.isAutohinted();
    str << ", isDither=" << flags.isDither();
    str << ", textEncoding="
        << PaintOpHelper::SkiaTypeToString(flags.getTextEncoding());
    str << ", textSize="
        << PaintOpHelper::SkiaTypeToString(flags.getTextSize());
    str << ", filterQuality="
        << PaintOpHelper::SkiaTypeToString(flags.getFilterQuality());
    str << ", strokeWidth="
        << PaintOpHelper::SkiaTypeToString(flags.getStrokeWidth());
    str << ", strokeMiter="
        << PaintOpHelper::SkiaTypeToString(flags.getStrokeMiter());
    str << ", strokeCap="
        << PaintOpHelper::SkiaTypeToString(flags.getStrokeCap());
    str << ", strokeJoin="
        << PaintOpHelper::SkiaTypeToString(flags.getStrokeJoin());
    str << ", typeface="
        << PaintOpHelper::SkiaTypeToString(flags.getTypeface());
    str << ", colorFilter="
        << PaintOpHelper::SkiaTypeToString(flags.getColorFilter());
    str << ", maskFilter="
        << PaintOpHelper::SkiaTypeToString(flags.getMaskFilter());
    str << ", shader=" << PaintOpHelper::PaintShaderToString(flags.getShader());
    str << ", hasShader=" << flags.HasShader();
    str << ", shaderIsOpaque=" << (flags.HasShader() && flags.ShaderIsOpaque());
    str << ", pathEffect="
        << PaintOpHelper::SkiaTypeToString(flags.getPathEffect());
    str << ", imageFilter="
        << PaintOpHelper::PaintFilterToString(flags.getImageFilter());
    str << ", drawLooper="
        << PaintOpHelper::SkiaTypeToString(flags.getLooper());
    str << ", isSimpleOpacity=" << flags.IsSimpleOpacity();
    str << ", supportsFoldingAlpha=" << flags.SupportsFoldingAlpha();
    str << ", isValid=" << flags.IsValid();
    str << ", hasDiscardableImages=" << flags.HasDiscardableImages();
    str << "]";
    return str.str();
  }
};

}  // namespace cc

inline ::std::ostream& operator<<(::std::ostream& os, const cc::PaintOp& op) {
  return os << cc::PaintOpHelper::ToString(&op);
}

#endif  // CC_TEST_PAINT_OP_HELPER_H_
