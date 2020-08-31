/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm/gm.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkColorPriv.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkFilterQuality.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageGenerator.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkScalar.h"
#include "include/core/SkSize.h"
#include "include/core/SkString.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkTypes.h"
#include "include/core/SkYUVAIndex.h"
#include "include/core/SkYUVASizeInfo.h"
#include "include/gpu/GrBackendSurface.h"
#include "include/gpu/GrConfig.h"
#include "include/gpu/GrContext.h"
#include "include/gpu/GrTypes.h"
#include "include/private/GrTypesPriv.h"
#include "include/private/SkTArray.h"
#include "include/private/SkTDArray.h"
#include "include/private/SkTemplates.h"
#include "include/utils/SkTextUtils.h"
#include "src/core/SkYUVMath.h"
#include "src/gpu/GrContextPriv.h"
#include "src/gpu/GrGpu.h"
#include "tools/ToolUtils.h"

#include <math.h>
#include <string.h>
#include <initializer_list>
#include <memory>
#include <utility>

class GrRenderTargetContext;

static const int kTileWidthHeight = 128;
static const int kLabelWidth = 64;
static const int kLabelHeight = 32;
static const int kDomainPadding = 8;
static const int kPad = 1;

enum YUVFormat {
    // 4:2:0 formats, 24 bpp
    kP016_YUVFormat, // 16-bit Y plane + 2x2 down sampled interleaved U/V plane (2 textures)
    // 4:2:0 formats, "15 bpp" (but really 24 bpp)
    kP010_YUVFormat, // same as kP016 except "10 bpp". Note that it is the same memory layout
                     // except that the bottom 6 bits are zeroed out (2 textures)
    // TODO: we're cheating a bit w/ P010 and just treating it as unorm 16. This means its
    // fully saturated values are 65504 rather than 65535 (that is just .9995 out of 1.0 though).

    // This is laid out the same as kP016 and kP010 but uses F16 unstead of U16. In this case
    // the 10 bits/channel vs 16 bits/channel distinction isn't relevant.
    kP016F_YUVFormat,

    // 4:4:4 formats, 64 bpp
    kY416_YUVFormat,  // 16-bit AVYU values all interleaved (1 texture)

    // 4:4:4 formats, 32 bpp
    kAYUV_YUVFormat,  // 8-bit YUVA values all interleaved (1 texture)
    kY410_YUVFormat,  // AVYU w/ 10bpp for YUV and 2 for A all interleaved (1 texture)

    // 4:2:0 formats, 12 bpp
    kNV12_YUVFormat, // 8-bit Y plane + 2x2 down sampled interleaved U/V planes (2 textures)
    kNV21_YUVFormat, // same as kNV12 but w/ U/V reversed in the interleaved texture (2 textures)

    kI420_YUVFormat, // 8-bit Y plane + separate 2x2 down sampled U and V planes (3 textures)
    kYV12_YUVFormat, // 8-bit Y plane + separate 2x2 down sampled V and U planes (3 textures)

    kLast_YUVFormat = kYV12_YUVFormat
};

class YUVAPlanarConfig {
public:
    struct YUVALocation {
        int fPlaneIdx = -1;
        int fChannelIdx = -1;
    };

    enum class YUVAChannel { kY, kU, kV, kA };

    explicit YUVAPlanarConfig(const std::initializer_list<YUVALocation>& yuvaLocations);

    constexpr int numPlanes() const { return fNumPlanes; }

    int planeIndex(YUVAChannel c) const { return fLocations[static_cast<int>(c)].fPlaneIdx; }

    int channelIndex(YUVAChannel c) const { return fLocations[static_cast<int>(c)].fChannelIdx; }

    constexpr bool hasAlpha() const { return fLocations[3].fPlaneIdx >= 0; }

    /**
     * Given a mask of SkColorChannelFlags choose a channel by index. Legal 'channelMask' values
     * are:
     *      kAlpha, kGray, kRed, kRG, kRGB, kRGBA.
     * The channel index must be less than the number of bits set in the mask. The index order is
     * the order listed above (e.g. if 'channelMask' is kRGB and 'channelIdx' is 1 then
     * SkColorChannel::kG is returned as 'channel'). The function fails if 'channelMask' is not one
     * of the listed allowed values or 'channelIdx' is invalid for the mask.
     */
    static bool ChannelIndexToChannel(uint32_t channelMask,
                                      int channelIdx,
                                      SkColorChannel* channel);

    /**
     * Goes from channel indices to actual channels given texture formats. Also supports adding
     * on an external alpha plane if this format doesn't already have alpha. The extra alpha texture
     * must be the last texture and the channel index is assumed to be 0.
     */
    bool getYUVAIndices(const GrBackendTexture textures[],
                        int numTextures,
                        bool externalAlphaPlane,
                        SkYUVAIndex indices[4]) const;

    /** Same as above but with pixmaps instead of textures. */
    bool getYUVAIndices(const SkBitmap planes[],
                        int numBitmaps,
                        bool externalAlphaPlane,
                        SkYUVAIndex indices[4]) const;

private:
    bool getYUVAIndices(const uint32_t channelMasks[],
                        int numPlanes,
                        bool externalAlphaPlane,
                        SkYUVAIndex indices[4]) const;

    YUVALocation fLocations[4] = {};
    int fNumPlanes = 0;
};

YUVAPlanarConfig::YUVAPlanarConfig(const std::initializer_list<YUVALocation>& yuvaLocations) {
    SkASSERT(yuvaLocations.size() == 3 || yuvaLocations.size() == 4);
    uint32_t planeMask[5] = {};
    int l = 0;
    for (const auto& location : yuvaLocations) {
        SkASSERT(location.fChannelIdx >= 0 && location.fChannelIdx <= 3);
        SkASSERT(location.fPlaneIdx >= 0 && location.fPlaneIdx <= 3);
        fLocations[l++] = location;
        fNumPlanes = std::max(fNumPlanes, location.fPlaneIdx + 1);
        int mask = 1 << location.fChannelIdx;
        SkASSERT(!(planeMask[location.fPlaneIdx] & mask));
        planeMask[location.fPlaneIdx] |= mask;
    }

    // Check that no plane is skipped and channel usage in each plane is tightly packed.
    for (int i = 0; i < fNumPlanes; ++i) {
        switch (planeMask[i]) {
            case 0b0001: break;
            case 0b0011: break;
            case 0b0111: break;
            case 0b1111: break;
            default:     SK_ABORT("Illegal channel configuration. "
                                  "Maximum of 4 channels per plane. "
                                  "No skipped channels in any plane.");
        }
    }
}

bool YUVAPlanarConfig::ChannelIndexToChannel(uint32_t channelFlags,
                                             int channelIdx,
                                             SkColorChannel* channel) {
    switch (channelFlags) {
        case kGray_SkColorChannelFlag:  // For gray returning any of R, G, or B for index 0 is ok.
        case kRed_SkColorChannelFlag:
            if (channelIdx == 0) {
                *channel = SkColorChannel::kR;
                return true;
            }
            return false;
        case kAlpha_SkColorChannelFlag:
            if (channelIdx == 0) {
                *channel = SkColorChannel::kA;
                return true;
            }
            return false;
        case kRG_SkColorChannelFlags:
            if (channelIdx == 0 || channelIdx == 1) {
                *channel = static_cast<SkColorChannel>(channelIdx);
                return true;
            }
            return false;
        case kRGB_SkColorChannelFlags:
            if (channelIdx >= 0 && channelIdx <= 2) {
                *channel = static_cast<SkColorChannel>(channelIdx);
                return true;
            }
            return false;
        case kRGBA_SkColorChannelFlags:
            if (channelIdx >= 0 && channelIdx <= 3) {
                *channel = static_cast<SkColorChannel>(channelIdx);
                return true;
            }
            return false;
        default:
            return false;
    }
}

bool YUVAPlanarConfig::getYUVAIndices(const GrBackendTexture textures[],
                                      int numTextures,
                                      bool externalAlphaPlane,
                                      SkYUVAIndex indices[4]) const {
    uint32_t channelMasks[4] = {};
    for (int i = 0; i < numTextures; ++i) {
        channelMasks[i] = textures[i].getBackendFormat().channelMask();
    }
    return this->getYUVAIndices(channelMasks, numTextures, externalAlphaPlane, indices);
}

bool YUVAPlanarConfig::getYUVAIndices(const SkBitmap bitmaps[],
                                      int numBitmaps,
                                      bool externalAlphaPlane,
                                      SkYUVAIndex indices[4]) const {
    uint32_t channelMasks[4] = {};
    for (int i = 0; i < numBitmaps; ++i) {
        channelMasks[i] = SkColorTypeChannelFlags(bitmaps[i].colorType());
    }
    return this->getYUVAIndices(channelMasks, numBitmaps, externalAlphaPlane, indices);
}

bool YUVAPlanarConfig::getYUVAIndices(const uint32_t planeChannelMasks[],
                                      int numPlanes,
                                      bool externalAlphaPlane,
                                      SkYUVAIndex indices[4]) const {
    if (this->hasAlpha() && externalAlphaPlane) {
        return false;
    }
    if (numPlanes != fNumPlanes + SkToInt(externalAlphaPlane)) {
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        int plane = fLocations[i].fPlaneIdx;
        if (plane < 0) {
            indices[i].fIndex = -1;
            indices[i].fChannel = SkColorChannel::kR;
        } else {
            indices[i].fIndex = plane;
            if (!ChannelIndexToChannel(planeChannelMasks[plane], fLocations[i].fChannelIdx,
                                       &indices[i].fChannel)) {
                return false;
            }
        }
    }
    if (externalAlphaPlane) {
        if (!ChannelIndexToChannel(planeChannelMasks[numPlanes - 1], 0, &indices[3].fChannel)) {
            return false;
        }
        indices[3].fIndex = numPlanes - 1;
    }
    SkDEBUGCODE(int checkNumPlanes;)
    SkASSERT(SkYUVAIndex::AreValidIndices(indices, &checkNumPlanes));
    SkASSERT(checkNumPlanes == numPlanes);
    return true;
}

static const YUVAPlanarConfig& YUVAFormatPlanarConfig(YUVFormat format) {
    switch (format) {
        case kP016_YUVFormat:  // These all share the same plane/channel indices.
        case kP010_YUVFormat:
        case kP016F_YUVFormat: {
            static const YUVAPlanarConfig kConfig({{0, 0}, {1, 0}, {1, 1}});
            return kConfig;
        }
        case kY416_YUVFormat: {
            static const YUVAPlanarConfig kConfig({{0, 1}, {0, 0}, {0, 2}, {0, 3}});
            return kConfig;
        }
        case kAYUV_YUVFormat: {
            static const YUVAPlanarConfig kConfig({{0, 0}, {0, 1}, {0, 2}, {0, 3}});
            return kConfig;
        }
        case kY410_YUVFormat: {
            static const YUVAPlanarConfig kConfig({{0, 1}, {0, 0}, {0, 2}, {0, 3}});
            return kConfig;
        }
        case kNV12_YUVFormat: {
            static const YUVAPlanarConfig kConfig({{0, 0}, {1, 0}, {1, 1}});
            return kConfig;
        }
        case kNV21_YUVFormat: {
            static const YUVAPlanarConfig kConfig({{0, 0}, {1, 1}, {1, 0}});
            return kConfig;
        }
        case kI420_YUVFormat: {
            static const YUVAPlanarConfig kConfig({{0, 0}, {1, 0}, {2, 0}});
            return kConfig;
        }
        case kYV12_YUVFormat: {
            static const YUVAPlanarConfig kConfig({{0, 0}, {2, 0}, {1, 0}});
            return kConfig;
        }
    }
    SkUNREACHABLE;
}

static bool is_colorType_texturable(const GrCaps* caps, GrColorType ct) {
    GrBackendFormat format = caps->getDefaultBackendFormat(ct, GrRenderable::kNo);
    if (!format.isValid()) {
        return false;
    }

    return caps->isFormatTexturable(format);
}

static bool is_format_natively_supported(GrContext* context, YUVFormat yuvFormat) {

    const GrCaps* caps = context->priv().caps();

    switch (yuvFormat) {
        case kP016_YUVFormat:  // fall through
        case kP010_YUVFormat:  return is_colorType_texturable(caps, GrColorType::kAlpha_16) &&
                                      is_colorType_texturable(caps, GrColorType::kRG_1616);
        case kP016F_YUVFormat: return is_colorType_texturable(caps, GrColorType::kAlpha_F16) &&
                                      is_colorType_texturable(caps, GrColorType::kRG_F16);
        case kY416_YUVFormat:  return is_colorType_texturable(caps, GrColorType::kRGBA_16161616);
        case kAYUV_YUVFormat:  return is_colorType_texturable(caps, GrColorType::kRGBA_8888);
        case kY410_YUVFormat:  return is_colorType_texturable(caps, GrColorType::kRGBA_1010102);
        case kNV12_YUVFormat:  // fall through
        case kNV21_YUVFormat:  return is_colorType_texturable(caps, GrColorType::kGray_8) &&
                                      is_colorType_texturable(caps, GrColorType::kRG_88);
        case kI420_YUVFormat: // fall through
        case kYV12_YUVFormat: return is_colorType_texturable(caps, GrColorType::kGray_8);
    }

    SkUNREACHABLE;
}

// All the planes we need to construct the various YUV formats
struct PlaneData {
   SkBitmap fYFull;
   SkBitmap fUFull;
   SkBitmap fVFull;
   SkBitmap fAFull;
   SkBitmap fUQuarter; // 2x2 downsampled U channel
   SkBitmap fVQuarter; // 2x2 downsampled V channel

   SkBitmap fFull;
   SkBitmap fQuarter; // 2x2 downsampled YUVA
};

// Add a portion of a circle to 'path'. The points 'o1' and 'o2' are on the border of the circle
// and have tangents 'v1' and 'v2'.
static void add_arc(SkPath* path,
                    const SkPoint& o1, const SkVector& v1,
                    const SkPoint& o2, const SkVector& v2,
                    SkTDArray<SkRect>* circles, bool takeLongWayRound) {

    SkVector v3 = { -v1.fY, v1.fX };
    SkVector v4 = { v2.fY, -v2.fX };

    SkScalar t = ((o2.fX - o1.fX) * v4.fY - (o2.fY - o1.fY) * v4.fX) / v3.cross(v4);
    SkPoint center = { o1.fX + t * v3.fX, o1.fY + t * v3.fY };

    SkRect r = { center.fX - t, center.fY - t, center.fX + t, center.fY + t };

    if (circles) {
        circles->push_back(r);
    }

    SkVector startV = o1 - center, endV = o2 - center;
    startV.normalize();
    endV.normalize();

    SkScalar startDeg = SkRadiansToDegrees(SkScalarATan2(startV.fY, startV.fX));
    SkScalar endDeg = SkRadiansToDegrees(SkScalarATan2(endV.fY, endV.fX));

    startDeg += 360.0f;
    startDeg = fmodf(startDeg, 360.0f);

    endDeg += 360.0f;
    endDeg = fmodf(endDeg, 360.0f);

    if (endDeg < startDeg) {
        endDeg += 360.0f;
    }

    SkScalar sweepDeg = SkTAbs(endDeg - startDeg);
    if (!takeLongWayRound) {
        sweepDeg = sweepDeg - 360;
    }

    path->arcTo(r, startDeg, sweepDeg, false);
}

static SkPath create_splat(const SkPoint& o, SkScalar innerRadius, SkScalar outerRadius,
                           SkScalar ratio, int numLobes, SkTDArray<SkRect>* circles) {
    if (numLobes <= 1) {
        return SkPath();
    }

    SkPath p;

    int numDivisions = 2 * numLobes;
    SkScalar fullLobeDegrees = 360.0f / numLobes;
    SkScalar outDegrees = ratio * fullLobeDegrees / (ratio + 1.0f);
    SkScalar innerDegrees = fullLobeDegrees / (ratio + 1.0f);
    SkMatrix outerStep, innerStep;
    outerStep.setRotate(outDegrees);
    innerStep.setRotate(innerDegrees);
    SkVector curV = SkVector::Make(0.0f, 1.0f);

    if (circles) {
        circles->push_back(SkRect::MakeLTRB(o.fX - innerRadius, o.fY - innerRadius,
                                            o.fX + innerRadius, o.fY + innerRadius));
    }

    p.moveTo(o.fX + innerRadius * curV.fX, o.fY + innerRadius * curV.fY);

    for (int i = 0; i < numDivisions; ++i) {

        SkVector nextV;
        if (0 == (i % 2)) {
            nextV = outerStep.mapVector(curV.fX, curV.fY);

            SkPoint top = SkPoint::Make(o.fX + outerRadius * curV.fX,
                                        o.fY + outerRadius * curV.fY);
            SkPoint nextTop = SkPoint::Make(o.fX + outerRadius * nextV.fX,
                                            o.fY + outerRadius * nextV.fY);

            p.lineTo(top);
            add_arc(&p, top, curV, nextTop, nextV, circles, true);
        } else {
            nextV = innerStep.mapVector(curV.fX, curV.fY);

            SkPoint bot = SkPoint::Make(o.fX + innerRadius * curV.fX,
                                        o.fY + innerRadius * curV.fY);
            SkPoint nextBot = SkPoint::Make(o.fX + innerRadius * nextV.fX,
                                            o.fY + innerRadius * nextV.fY);

            p.lineTo(bot);
            add_arc(&p, bot, curV, nextBot, nextV, nullptr, false);
        }

        curV = nextV;
    }

    p.close();

    return p;
}

static SkBitmap make_bitmap(SkColorType colorType, const SkPath& path,
                            const SkTDArray<SkRect>& circles, bool opaque, bool padWithRed) {
    const SkColor kGreen  = ToolUtils::color_to_565(SkColorSetARGB(0xFF, 178, 240, 104));
    const SkColor kBlue   = ToolUtils::color_to_565(SkColorSetARGB(0xFF, 173, 167, 252));
    const SkColor kYellow = ToolUtils::color_to_565(SkColorSetARGB(0xFF, 255, 221, 117));

    int widthHeight = kTileWidthHeight + (padWithRed ? 2 * kDomainPadding : 0);

    SkImageInfo ii = SkImageInfo::Make(widthHeight, widthHeight,
                                       colorType, kPremul_SkAlphaType);

    SkBitmap bm;
    bm.allocPixels(ii);

    std::unique_ptr<SkCanvas> canvas = SkCanvas::MakeRasterDirect(ii,
                                                                  bm.getPixels(),
                                                                  bm.rowBytes());
    if (padWithRed) {
        canvas->clear(SK_ColorRED);
        canvas->translate(kDomainPadding, kDomainPadding);
        canvas->clipRect(SkRect::MakeWH(kTileWidthHeight, kTileWidthHeight));
    }
    canvas->clear(opaque ? kGreen : SK_ColorTRANSPARENT);

    SkPaint paint;
    paint.setAntiAlias(false); // serialize-8888 doesn't seem to work well w/ partial transparency
    paint.setColor(kBlue);

    canvas->drawPath(path, paint);

    paint.setColor(opaque ? kYellow : SK_ColorTRANSPARENT);
    paint.setBlendMode(SkBlendMode::kSrc);
    for (int i = 0; i < circles.count(); ++i) {
        SkRect r = circles[i];
        r.inset(r.width()/4, r.height()/4);
        canvas->drawOval(r, paint);
    }

    return bm;
}

static void convert_rgba_to_yuva(const float mtx[20], SkColor col, uint8_t yuv[4]) {
    const uint8_t r = SkColorGetR(col);
    const uint8_t g = SkColorGetG(col);
    const uint8_t b = SkColorGetB(col);

    yuv[0] = SkTPin(SkScalarRoundToInt(mtx[ 0]*r + mtx[ 1]*g + mtx[ 2]*b + mtx[ 4]*255), 0, 255);
    yuv[1] = SkTPin(SkScalarRoundToInt(mtx[ 5]*r + mtx[ 6]*g + mtx[ 7]*b + mtx[ 9]*255), 0, 255);
    yuv[2] = SkTPin(SkScalarRoundToInt(mtx[10]*r + mtx[11]*g + mtx[12]*b + mtx[14]*255), 0, 255);
    yuv[3] = SkColorGetA(col);
}

static SkPMColor convert_yuva_to_rgba(const float mtx[20], uint8_t yuva[4]) {
    uint8_t y = yuva[0];
    uint8_t u = yuva[1];
    uint8_t v = yuva[2];
    uint8_t a = yuva[3];

    uint8_t r = SkTPin(SkScalarRoundToInt(mtx[ 0]*y + mtx[ 1]*u + mtx[ 2]*v + mtx[ 4]*255), 0, 255);
    uint8_t g = SkTPin(SkScalarRoundToInt(mtx[ 5]*y + mtx[ 6]*u + mtx[ 7]*v + mtx[ 9]*255), 0, 255);
    uint8_t b = SkTPin(SkScalarRoundToInt(mtx[10]*y + mtx[11]*u + mtx[12]*v + mtx[14]*255), 0, 255);

    return SkPremultiplyARGBInline(a, r, g, b);
}

static void extract_planes(const SkBitmap& bm, SkYUVColorSpace yuvColorSpace, PlaneData* planes) {
    if (kIdentity_SkYUVColorSpace == yuvColorSpace) {
        // To test the identity color space we use JPEG YUV planes
        yuvColorSpace = kJPEG_SkYUVColorSpace;
    }

    SkASSERT(!(bm.width() % 2));
    SkASSERT(!(bm.height() % 2));
    planes->fYFull.allocPixels(
            SkImageInfo::Make(bm.dimensions(), kGray_8_SkColorType, kUnpremul_SkAlphaType));
    planes->fUFull.allocPixels(
            SkImageInfo::Make(bm.dimensions(), kGray_8_SkColorType, kUnpremul_SkAlphaType));
    planes->fVFull.allocPixels(
            SkImageInfo::Make(bm.dimensions(), kGray_8_SkColorType, kUnpremul_SkAlphaType));
    planes->fAFull.allocPixels(SkImageInfo::MakeA8(bm.width(), bm.height()));
    planes->fUQuarter.allocPixels(SkImageInfo::Make(bm.width()/2, bm.height()/2,
                                  kGray_8_SkColorType, kUnpremul_SkAlphaType));
    planes->fVQuarter.allocPixels(SkImageInfo::Make(bm.width()/2, bm.height()/2,
                                  kGray_8_SkColorType, kUnpremul_SkAlphaType));

    planes->fFull.allocPixels(
            SkImageInfo::Make(bm.dimensions(), kRGBA_F32_SkColorType, kUnpremul_SkAlphaType));
    planes->fQuarter.allocPixels(SkImageInfo::Make(bm.width()/2, bm.height()/2,
                                 kRGBA_F32_SkColorType, kUnpremul_SkAlphaType));

    float mtx[20];
    SkColorMatrix_RGB2YUV(yuvColorSpace, mtx);

    SkColor4f* dst = (SkColor4f *) planes->fFull.getAddr(0, 0);
    for (int y = 0; y < bm.height(); ++y) {
        for (int x = 0; x < bm.width(); ++x) {
            SkColor col = bm.getColor(x, y);

            uint8_t yuva[4];

            convert_rgba_to_yuva(mtx, col, yuva);

            *planes->fYFull.getAddr8(x, y) = yuva[0];
            *planes->fUFull.getAddr8(x, y) = yuva[1];
            *planes->fVFull.getAddr8(x, y) = yuva[2];
            *planes->fAFull.getAddr8(x, y) = yuva[3];

            // TODO: render in F32 rather than converting here
            dst->fR = yuva[0] / 255.0f;
            dst->fG = yuva[1] / 255.0f;
            dst->fB = yuva[2] / 255.0f;
            dst->fA = yuva[3] / 255.0f;
            ++dst;
        }
    }

    dst = (SkColor4f *) planes->fQuarter.getAddr(0, 0);
    for (int y = 0; y < bm.height()/2; ++y) {
        for (int x = 0; x < bm.width()/2; ++x) {
            uint32_t yAccum = 0, uAccum = 0, vAccum = 0, aAccum = 0;

            yAccum += *planes->fYFull.getAddr8(2*x, 2*y);
            yAccum += *planes->fYFull.getAddr8(2*x+1, 2*y);
            yAccum += *planes->fYFull.getAddr8(2*x, 2*y+1);
            yAccum += *planes->fYFull.getAddr8(2*x+1, 2*y+1);

            uAccum += *planes->fUFull.getAddr8(2*x, 2*y);
            uAccum += *planes->fUFull.getAddr8(2*x+1, 2*y);
            uAccum += *planes->fUFull.getAddr8(2*x, 2*y+1);
            uAccum += *planes->fUFull.getAddr8(2*x+1, 2*y+1);

            *planes->fUQuarter.getAddr8(x, y) = uAccum / 4.0f;

            vAccum += *planes->fVFull.getAddr8(2*x, 2*y);
            vAccum += *planes->fVFull.getAddr8(2*x+1, 2*y);
            vAccum += *planes->fVFull.getAddr8(2*x, 2*y+1);
            vAccum += *planes->fVFull.getAddr8(2*x+1, 2*y+1);

            *planes->fVQuarter.getAddr8(x, y) = vAccum / 4.0f;

            aAccum += *planes->fAFull.getAddr8(2*x, 2*y);
            aAccum += *planes->fAFull.getAddr8(2*x+1, 2*y);
            aAccum += *planes->fAFull.getAddr8(2*x, 2*y+1);
            aAccum += *planes->fAFull.getAddr8(2*x+1, 2*y+1);

            // TODO: render in F32 rather than converting here
            dst->fR = yAccum / (4.0f * 255.0f);
            dst->fG = uAccum / (4.0f * 255.0f);
            dst->fB = vAccum / (4.0f * 255.0f);
            dst->fA = aAccum / (4.0f * 255.0f);
            ++dst;
        }
    }
}

// Create a 2x2 downsampled SkBitmap. It is stored in an RG texture. It can optionally be
// uv (i.e., NV12) or vu (i.e., NV21).
static SkBitmap make_quarter_2_channel(const SkBitmap& fullY,
                                       const SkBitmap& quarterU,
                                       const SkBitmap& quarterV,
                                       bool uv) {
    SkBitmap result;

    result.allocPixels(SkImageInfo::Make(fullY.width()/2,
                                         fullY.height()/2,
                                         kR8G8_unorm_SkColorType,
                                         kUnpremul_SkAlphaType));

    for (int y = 0; y < fullY.height()/2; ++y) {
        for (int x = 0; x < fullY.width()/2; ++x) {
            uint8_t u8 = *quarterU.getAddr8(x, y);
            uint8_t v8 = *quarterV.getAddr8(x, y);

            if (uv) {
                *result.getAddr16(x, y) = (v8 << 8) | u8;
            } else {
                *result.getAddr16(x, y) = (u8 << 8) | v8;
            }
        }
    }

    return result;
}

// Create some flavor of a 16bits/channel bitmap from a RGBA_F32 source
static SkBitmap make_16(const SkBitmap& src, SkColorType dstCT,
                        std::function<void(uint16_t* dstPixel, const float* srcPixel)> convert) {
    SkASSERT(src.colorType() == kRGBA_F32_SkColorType);

    SkBitmap result;

    result.allocPixels(SkImageInfo::Make(src.dimensions(), dstCT, kUnpremul_SkAlphaType));

    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            const float* srcPixel = (const float*) src.getAddr(x, y);
            uint16_t* dstPixel = (uint16_t*) result.getAddr(x, y);

            convert(dstPixel, srcPixel);
        }
    }

    return result;
}

static uint16_t flt_2_uint16(float flt) { return SkScalarRoundToInt(flt * 65535.0f); }

// Recombine the separate planes into some YUV format. Returns the number of planes.
static int create_YUV(const PlaneData& planes,
                      YUVFormat yuvFormat,
                      SkBitmap resultBMs[],
                      bool opaque) {
    int nextLayer = 0;

    switch (yuvFormat) {
        case kY416_YUVFormat: {
            resultBMs[nextLayer++] = make_16(planes.fFull, kR16G16B16A16_unorm_SkColorType,
                                             [] (uint16_t* dstPixel, const float* srcPixel) {
                                                 dstPixel[0] = flt_2_uint16(srcPixel[1]); // U
                                                 dstPixel[1] = flt_2_uint16(srcPixel[0]); // Y
                                                 dstPixel[2] = flt_2_uint16(srcPixel[2]); // V
                                                 dstPixel[3] = flt_2_uint16(srcPixel[3]); // A
                                             });
            break;
        }
        case kAYUV_YUVFormat: {
            SkBitmap yuvaFull;

            yuvaFull.allocPixels(SkImageInfo::Make(planes.fYFull.width(), planes.fYFull.height(),
                                                   kRGBA_8888_SkColorType, kUnpremul_SkAlphaType));

            for (int y = 0; y < planes.fYFull.height(); ++y) {
                for (int x = 0; x < planes.fYFull.width(); ++x) {

                    uint8_t Y = *planes.fYFull.getAddr8(x, y);
                    uint8_t U = *planes.fUFull.getAddr8(x, y);
                    uint8_t V = *planes.fVFull.getAddr8(x, y);
                    uint8_t A = *planes.fAFull.getAddr8(x, y);

                    // NOT premul!
                    // V and Y swapped to match RGBA layout
                    SkColor c = SkColorSetARGB(A, V, U, Y);
                    *yuvaFull.getAddr32(x, y) = c;
                }
            }

            resultBMs[nextLayer++] = yuvaFull;
            break;
        }
        case kY410_YUVFormat: {
            SkBitmap yuvaFull;
            uint32_t Y, U, V;
            uint8_t A;

            yuvaFull.allocPixels(SkImageInfo::Make(planes.fYFull.width(), planes.fYFull.height(),
                                                   kRGBA_1010102_SkColorType,
                                                   kUnpremul_SkAlphaType));

            for (int y = 0; y < planes.fYFull.height(); ++y) {
                for (int x = 0; x < planes.fYFull.width(); ++x) {

                    Y = SkScalarRoundToInt((*planes.fYFull.getAddr8(x, y) / 255.0f) * 1023.0f);
                    U = SkScalarRoundToInt((*planes.fUFull.getAddr8(x, y) / 255.0f) * 1023.0f);
                    V = SkScalarRoundToInt((*planes.fVFull.getAddr8(x, y) / 255.0f) * 1023.0f);
                    A = SkScalarRoundToInt((*planes.fAFull.getAddr8(x, y) / 255.0f) * 3.0f);

                    // NOT premul!
                    *yuvaFull.getAddr32(x, y) = (A << 30) | (V << 20) | (Y << 10) | (U << 0);
                }
            }

            resultBMs[nextLayer++] = yuvaFull;
            break;
        }
        case kP016_YUVFormat:     // fall through
        case kP010_YUVFormat: {
            resultBMs[nextLayer++] = make_16(planes.fFull, kA16_unorm_SkColorType,
                                             [tenBitsPP = (yuvFormat == kP010_YUVFormat)]
                                             (uint16_t* dstPixel, const float* srcPixel) {
                                                 uint16_t val16 = flt_2_uint16(srcPixel[0]);
                                                 dstPixel[0] = tenBitsPP ? (val16 & 0xFFC0)
                                                                         : val16;
                                              });
            resultBMs[nextLayer++] = make_16(planes.fQuarter, kR16G16_unorm_SkColorType,
                                             [tenBitsPP = (yuvFormat == kP010_YUVFormat)]
                                             (uint16_t* dstPixel, const float* srcPixel) {
                                                 uint16_t u16 = flt_2_uint16(srcPixel[1]);
                                                 uint16_t v16 = flt_2_uint16(srcPixel[2]);
                                                 dstPixel[0] = tenBitsPP ? (u16 & 0xFFC0) : u16;
                                                 dstPixel[1] = tenBitsPP ? (v16 & 0xFFC0) : v16;
                                             });
            if (!opaque) {
                resultBMs[nextLayer++] = make_16(planes.fFull, kA16_unorm_SkColorType,
                                                 [tenBitsPP = (yuvFormat == kP010_YUVFormat)]
                                                 (uint16_t* dstPixel, const float* srcPixel) {
                                                     uint16_t val16 = flt_2_uint16(srcPixel[3]);
                                                     dstPixel[0] = tenBitsPP ? (val16 & 0xFFC0)
                                                                             : val16;
                                                 });
            }
            return nextLayer;
        }
        case kP016F_YUVFormat: {
            resultBMs[nextLayer++] = make_16(planes.fFull, kA16_float_SkColorType,
                                             [] (uint16_t* dstPixel, const float* srcPixel) {
                                                 dstPixel[0] = SkFloatToHalf(srcPixel[0]);
                                             });
            resultBMs[nextLayer++] = make_16(planes.fQuarter, kR16G16_float_SkColorType,
                                             [] (uint16_t* dstPixel, const float* srcPixel) {
                                                 dstPixel[0] = SkFloatToHalf(srcPixel[1]);
                                                 dstPixel[1] = SkFloatToHalf(srcPixel[2]);
                                             });
            if (!opaque) {
                resultBMs[nextLayer++] = make_16(planes.fFull, kA16_float_SkColorType,
                                                 [] (uint16_t* dstPixel, const float* srcPixel) {
                                                     dstPixel[0] = SkFloatToHalf(srcPixel[3]);
                                                 });
            }
            return nextLayer;
        }
        case kNV12_YUVFormat: {
            SkBitmap uvQuarter = make_quarter_2_channel(planes.fYFull,
                                                        planes.fUQuarter,
                                                        planes.fVQuarter, true);
            resultBMs[nextLayer++] = planes.fYFull;
            resultBMs[nextLayer++] = uvQuarter;
            break;
        }
        case kNV21_YUVFormat: {
            SkBitmap vuQuarter = make_quarter_2_channel(planes.fYFull,
                                                        planes.fUQuarter,
                                                        planes.fVQuarter, false);
            resultBMs[nextLayer++] = planes.fYFull;
            resultBMs[nextLayer++] = vuQuarter;
            break;
        }
        case kI420_YUVFormat:
            resultBMs[nextLayer++] = planes.fYFull;
            resultBMs[nextLayer++] = planes.fUQuarter;
            resultBMs[nextLayer++] = planes.fVQuarter;
            break;
        case kYV12_YUVFormat:
            resultBMs[nextLayer++] = planes.fYFull;
            resultBMs[nextLayer++] = planes.fVQuarter;
            resultBMs[nextLayer++] = planes.fUQuarter;
            break;
    }

    if (!YUVAFormatPlanarConfig(yuvFormat).hasAlpha() && !opaque) {
        resultBMs[nextLayer++] = planes.fAFull;
    }
    return nextLayer;
}

static uint8_t look_up(float x1, float y1, const SkBitmap& bm, int channelIdx) {
    SkASSERT(x1 > 0 && x1 < 1.0f);
    SkASSERT(y1 > 0 && y1 < 1.0f);
    int x = SkScalarFloorToInt(x1 * bm.width());
    int y = SkScalarFloorToInt(y1 * bm.height());

    auto channelMask = SkColorTypeChannelFlags(bm.colorType());
    SkColorChannel channel;
    SkAssertResult(YUVAPlanarConfig::ChannelIndexToChannel(channelMask, channelIdx, &channel));
    auto ii = SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, bm.alphaType(), bm.refColorSpace());
    uint32_t pixel;
    SkAssertResult(bm.readPixels(ii, &pixel, sizeof(pixel), x, y));
    int shift = static_cast<int>(channel) * 8;
    return static_cast<uint8_t>((pixel >> shift) & 0xff);
}

class YUVGenerator : public SkImageGenerator {
public:
    YUVGenerator(const SkImageInfo& ii,
                 SkYUVColorSpace yuvColorSpace,
                 YUVFormat yuvFormat,
                 bool externalAlphaPlane,
                 SkBitmap bitmaps[SkYUVASizeInfo::kMaxCount])
            : SkImageGenerator(ii)
            , fYUVFormat(yuvFormat)
            , fYUVColorSpace(yuvColorSpace)
            , fExternalAlphaPlane(externalAlphaPlane)
            , fAllA8(true) {
        SkASSERT(!externalAlphaPlane || !YUVAFormatPlanarConfig(fYUVFormat).hasAlpha());
        int numPlanes = this->numPlanes();
        for (int i = 0; i < numPlanes; ++i) {
            fYUVBitmaps[i] = bitmaps[i];
            if (kAlpha_8_SkColorType != fYUVBitmaps[i].colorType()) {
                fAllA8 = false;
            }
        }
    }

protected:
    bool onGetPixels(const SkImageInfo& info, void* pixels, size_t rowBytes,
                     const Options&) override {

        if (kUnknown_SkColorType == fFlattened.colorType()) {
            fFlattened.allocPixels(info);
            SkASSERT(kN32_SkColorType == info.colorType());
            SkASSERT(kPremul_SkAlphaType == info.alphaType());

            float mtx[20];
            SkColorMatrix_YUV2RGB(fYUVColorSpace, mtx);

            for (int y = 0; y < info.height(); ++y) {
                for (int x = 0; x < info.width(); ++x) {

                    float x1 = (x + 0.5f) / info.width();
                    float y1 = (y + 0.5f) / info.height();

                    uint8_t yuva[4] = {0, 0, 0, 255};

                    const auto& planarConfig = YUVAFormatPlanarConfig(fYUVFormat);
                    using YUVAChannel = YUVAPlanarConfig::YUVAChannel;
                    for (auto c : {YUVAChannel::kY, YUVAChannel::kU, YUVAChannel::kV}) {
                        const auto& bmp = fYUVBitmaps[planarConfig.planeIndex(c)];
                        int channelIdx = planarConfig.channelIndex(c);
                        yuva[static_cast<int>(c)] = look_up(x1, y1, bmp, channelIdx);
                    }
                    if (planarConfig.hasAlpha()) {
                        const auto& bmp = fYUVBitmaps[planarConfig.planeIndex(YUVAChannel::kA)];
                        int channelIdx = planarConfig.channelIndex(YUVAChannel::kA);
                        yuva[3] = look_up(x1, y1, bmp, channelIdx);
                    } else if (fExternalAlphaPlane) {
                        const auto& bmp = fYUVBitmaps[this->numPlanes() - 1];
                        yuva[3] = look_up(x1, y1, bmp, 0);
                    }

                    // Making premul here.
                    *fFlattened.getAddr32(x, y) = convert_yuva_to_rgba(mtx, yuva);
                }
            }
        }

        return fFlattened.readPixels(info, pixels, rowBytes, 0, 0);
    }

    bool onQueryYUVA8(SkYUVASizeInfo* size,
                      SkYUVAIndex yuvaIndices[SkYUVAIndex::kIndexCount],
                      SkYUVColorSpace* yuvColorSpace) const override {

        if (!fAllA8) {
            return false;
        }
        const auto& planarConfig = YUVAFormatPlanarConfig(fYUVFormat);
        if (!planarConfig.getYUVAIndices(fYUVBitmaps, this->numPlanes(), fExternalAlphaPlane,
                                         yuvaIndices)) {
            return false;
        }
        *yuvColorSpace = fYUVColorSpace;

        int numPlanes = this->numPlanes();
        int i = 0;
        for (; i < numPlanes; ++i) {
            size->fSizes[i].fWidth = fYUVBitmaps[i].width();
            size->fSizes[i].fHeight = fYUVBitmaps[i].height();
            size->fWidthBytes[i] = fYUVBitmaps[i].rowBytes();
        }
        for ( ; i < SkYUVASizeInfo::kMaxCount; ++i) {
            size->fSizes[i].fWidth = 0;
            size->fSizes[i].fHeight = 0;
            size->fWidthBytes[i] = 0;
        }

        return true;
    }

    bool onGetYUVA8Planes(const SkYUVASizeInfo&, const SkYUVAIndex[SkYUVAIndex::kIndexCount],
                          void* planes[SkYUVASizeInfo::kMaxCount]) override {
        SkASSERT(fAllA8);
        int numPlanes = this->numPlanes();
        for (int i = 0; i < numPlanes; ++i) {
            planes[i] = fYUVBitmaps[i].getPixels();
        }
        return true;
    }

private:
    int numPlanes() const {
        return YUVAFormatPlanarConfig(fYUVFormat).numPlanes() + SkToInt(fExternalAlphaPlane);
    }

    YUVFormat       fYUVFormat;
    SkYUVColorSpace fYUVColorSpace;
    bool            fExternalAlphaPlane;
    SkBitmap        fYUVBitmaps[SkYUVASizeInfo::kMaxCount];
    SkBitmap        fFlattened;
    bool            fAllA8;     // are all the SkBitmaps in "fYUVBitmaps" A8?
};

static sk_sp<SkImage> make_yuv_gen_image(const SkImageInfo& ii,
                                         YUVFormat yuvFormat,
                                         SkYUVColorSpace yuvColorSpace,
                                         bool opaque,
                                         SkBitmap bitmaps[]) {
    bool externalAlphaPlane = !opaque && !YUVAFormatPlanarConfig(yuvFormat).hasAlpha();
    std::unique_ptr<SkImageGenerator> gen(
            new YUVGenerator(ii, yuvColorSpace, yuvFormat, externalAlphaPlane, bitmaps));

    return SkImage::MakeFromGenerator(std::move(gen));
}

static void draw_col_label(SkCanvas* canvas, int x, int yuvColorSpace, bool opaque) {
    static const char* kYUVColorSpaceNames[] = { "JPEG", "601", "709", "2020", "Identity" };
    static_assert(SK_ARRAY_COUNT(kYUVColorSpaceNames) == kLastEnum_SkYUVColorSpace + 1);

    SkPaint paint;
    SkFont  font(ToolUtils::create_portable_typeface(nullptr, SkFontStyle::Bold()), 16);
    font.setEdging(SkFont::Edging::kAlias);

    SkRect textRect;
    SkString colLabel;

    colLabel.printf("%s", kYUVColorSpaceNames[yuvColorSpace]);
    font.measureText(colLabel.c_str(), colLabel.size(), SkTextEncoding::kUTF8, &textRect);
    int y = textRect.height();

    SkTextUtils::DrawString(canvas, colLabel.c_str(), x, y, font, paint, SkTextUtils::kCenter_Align);

    colLabel.printf("%s", opaque ? "Opaque" : "Transparent");

    font.measureText(colLabel.c_str(), colLabel.size(), SkTextEncoding::kUTF8, &textRect);
    y += textRect.height();

    SkTextUtils::DrawString(canvas, colLabel.c_str(), x, y, font, paint, SkTextUtils::kCenter_Align);
}

static void draw_row_label(SkCanvas* canvas, int y, int yuvFormat) {
    static const char* kYUVFormatNames[] = {
        "P016", "P010", "P016F", "Y416", "AYUV", "Y410", "NV12", "NV21", "I420", "YV12"
    };
    static_assert(SK_ARRAY_COUNT(kYUVFormatNames) == kLast_YUVFormat + 1);

    SkPaint paint;
    SkFont  font(ToolUtils::create_portable_typeface(nullptr, SkFontStyle::Bold()), 16);
    font.setEdging(SkFont::Edging::kAlias);

    SkRect textRect;
    SkString rowLabel;

    rowLabel.printf("%s", kYUVFormatNames[yuvFormat]);
    font.measureText(rowLabel.c_str(), rowLabel.size(), SkTextEncoding::kUTF8, &textRect);
    y += kTileWidthHeight/2 + textRect.height()/2;

    canvas->drawString(rowLabel, 0, y, font, paint);
}

static GrBackendTexture create_yuva_texture(GrContext* context, const SkBitmap& bm) {
    return context->createBackendTexture(&bm.pixmap(), 1, GrRenderable::kNo, GrProtected::kNo);
}

static sk_sp<SkColorFilter> yuv_to_rgb_colorfilter() {
    static const float kJPEGConversionMatrix[20] = {
        1.0f,  0.0f,       1.402f,    0.0f, -180.0f/255,
        1.0f, -0.344136f, -0.714136f, 0.0f,  136.0f/255,
        1.0f,  1.772f,     0.0f,      0.0f, -227.6f/255,
        0.0f,  0.0f,       0.0f,      1.0f,    0.0f
    };

    return SkColorFilters::Matrix(kJPEGConversionMatrix);
}

// Get the SkColorType to use when creating an SkSurface wrapping 'format'.
static SkColorType get_color_type(const GrBackendFormat& format) {

    GrGLFormat glFormat = format.asGLFormat();
    if (GrGLFormat::kUnknown != glFormat) {
        switch (glFormat) {
            case GrGLFormat::kLUMINANCE8:   // fall through
            case GrGLFormat::kR8:           // fall through
            case GrGLFormat::kALPHA8:       return kAlpha_8_SkColorType;
            case GrGLFormat::kRG8:          return kR8G8_unorm_SkColorType;
            case GrGLFormat::kRGB8:         return kRGB_888x_SkColorType;
            case GrGLFormat::kRGBA8:        return kRGBA_8888_SkColorType;
            case GrGLFormat::kBGRA8:        return kBGRA_8888_SkColorType;
            case GrGLFormat::kRGB10_A2:     return kRGBA_1010102_SkColorType;
            case GrGLFormat::kLUMINANCE16F: // fall through
            case GrGLFormat::kR16F:         return kA16_float_SkColorType;
            case GrGLFormat::kRG16F:        return kR16G16_float_SkColorType;
            case GrGLFormat::kR16:          return kA16_unorm_SkColorType;
            case GrGLFormat::kRG16:         return kR16G16_unorm_SkColorType;
            case GrGLFormat::kRGBA16:       return kR16G16B16A16_unorm_SkColorType;
            default:                        return kUnknown_SkColorType;
        }

        SkUNREACHABLE;
    }

    VkFormat vkFormat;
    if (format.asVkFormat(&vkFormat)) {
        switch (vkFormat) {
            case VK_FORMAT_R8_UNORM:                 return kAlpha_8_SkColorType;
            case VK_FORMAT_R8G8_UNORM:               return kR8G8_unorm_SkColorType;
            case VK_FORMAT_R8G8B8_UNORM:             return kRGB_888x_SkColorType;
            case VK_FORMAT_R8G8B8A8_UNORM:           return kRGBA_8888_SkColorType;
            case VK_FORMAT_B8G8R8A8_UNORM:           return kBGRA_8888_SkColorType;
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return kRGBA_1010102_SkColorType;
            case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return kBGRA_1010102_SkColorType;
            case VK_FORMAT_R16_SFLOAT:               return kA16_float_SkColorType;
            case VK_FORMAT_R16G16_SFLOAT:            return kR16G16_float_SkColorType;
            case VK_FORMAT_R16_UNORM:                return kA16_unorm_SkColorType;
            case VK_FORMAT_R16G16_UNORM:             return kR16G16_unorm_SkColorType;
            case VK_FORMAT_R16G16B16A16_UNORM:       return kR16G16B16A16_unorm_SkColorType;
            default:                                 return kUnknown_SkColorType;
        }

        SkUNREACHABLE;
    }

    return kUnknown_SkColorType;
}

namespace skiagm {

// This GM creates an opaque and transparent bitmap, extracts the planes and then recombines
// them into various YUV formats. It then renders the results in the grid:
//
//                 JPEG                  601                   709                Identity
//        Transparent  Opaque   Transparent  Opaque   Transparent  Opaque   Transparent Opaque
// originals
// P016
// P010
// P016F
// Y416
// AYUV
// Y410
// NV12
// NV21
// I420
// YV12
class WackyYUVFormatsGM : public GM {
public:
    WackyYUVFormatsGM(bool useTargetColorSpace, bool useDomain, bool quarterSize)
            : fUseTargetColorSpace(useTargetColorSpace)
            , fUseDomain(useDomain)
            , fQuarterSize(quarterSize) {
        this->setBGColor(0xFFCCCCCC);
    }

protected:

    SkString onShortName() override {
        SkString name("wacky_yuv_formats");
        if (fUseTargetColorSpace) {
            name += "_cs";
        }
        if (fUseDomain) {
            name += "_domain";
        }
        if (fQuarterSize) {
            name += "_qtr";
        }

        return name;
    }

    SkISize onISize() override {
        int numCols = 2 * (kLastEnum_SkYUVColorSpace + 1); // opacity x #-color-spaces
        int numRows = 1 + (kLast_YUVFormat + 1);  // original + #-yuv-formats
        int wh = SkScalarCeilToInt(kTileWidthHeight * (fUseDomain ? 1.5f : 1.f));
        return SkISize::Make(kLabelWidth  + numCols * (wh + kPad),
                             kLabelHeight + numRows * (wh + kPad));
    }

    void onOnceBeforeDraw() override {
        SkPoint origin = { kTileWidthHeight/2.0f, kTileWidthHeight/2.0f };
        float outerRadius = kTileWidthHeight/2.0f - 20.0f;
        float innerRadius = 20.0f;

        {
            // transparent
            SkTDArray<SkRect> circles;
            SkPath path = create_splat(origin, innerRadius, outerRadius, 1.0f, 5, &circles);
            fOriginalBMs[0] = make_bitmap(kRGBA_8888_SkColorType, path, circles, false, fUseDomain);
        }

        {
            // opaque
            SkTDArray<SkRect> circles;
            SkPath path = create_splat(origin, innerRadius, outerRadius, 1.0f, 7, &circles);
            fOriginalBMs[1] = make_bitmap(kRGBA_8888_SkColorType, path, circles, true, fUseDomain);
        }

        if (fUseTargetColorSpace) {
            fTargetColorSpace = SkColorSpace::MakeSRGB()->makeColorSpin();
        }
    }

    // Resize all the backend textures in 'yuvaTextures' to a quarter their size.
    sk_sp<SkImage> resizeOnGpu(GrContext* context,
                               YUVFormat yuvFormat,
                               SkYUVColorSpace yuvColorSpace,
                               bool opaque,
                               const GrBackendTexture yuvaTextures[],
                               const SkYUVAIndex yuvaIndices[4],
                               int numTextures,
                               SkISize imageSize) {
        GrBackendTexture shrunkTextures[4];

        for (int i = 0; i < numTextures; ++i) {
            SkColorType ct = get_color_type(yuvaTextures[i].getBackendFormat());
            if (ct == kUnknown_SkColorType || !context->colorTypeSupportedAsSurface(ct)) {
                return nullptr;
            }

            if (ct == kRGBA_8888_SkColorType || ct == kRGBA_1010102_SkColorType) {
                // We disallow resizing AYUV and Y410 formats on the GPU bc resizing them w/ a
                // premul draw combines the YUV channels w/ the A channel in an inappropriate
                // manner.
                return nullptr;
            }

            SkISize shrunkPlaneSize = { yuvaTextures[i].width() / 2, yuvaTextures[i].height() / 2 };

            sk_sp<SkImage> wrappedOrig = SkImage::MakeFromTexture(context, yuvaTextures[i],
                                                                  kTopLeft_GrSurfaceOrigin,
                                                                  ct,
                                                                  kPremul_SkAlphaType,
                                                                  nullptr);

            shrunkTextures[i] = context->createBackendTexture(shrunkPlaneSize.width(),
                                                              shrunkPlaneSize.height(),
                                                              yuvaTextures[i].getBackendFormat(),
                                                              GrMipMapped::kNo,
                                                              GrRenderable::kYes);
            if (!shrunkTextures[i].isValid()) {
                return nullptr;
            }

            // Store this away so it will be cleaned up at the end.
            fBackendTextures.push_back(shrunkTextures[i]);

            sk_sp<SkSurface> s = SkSurface::MakeFromBackendTexture(context, shrunkTextures[i],
                                                                   kTopLeft_GrSurfaceOrigin, 0,
                                                                   ct, nullptr, nullptr);
            if (!s) {
                return nullptr;
            }
            SkCanvas* c = s->getCanvas();

            SkPaint paint;
            paint.setBlendMode(SkBlendMode::kSrc);

            c->drawImageRect(wrappedOrig,
                             SkRect::MakeWH(shrunkPlaneSize.width(), shrunkPlaneSize.height()),
                             &paint);

            s->flush();
        }

        SkISize shrunkImageSize = { imageSize.width() / 2, imageSize.height() / 2 };

        return SkImage::MakeFromYUVATextures(context,
                                             yuvColorSpace,
                                             shrunkTextures,
                                             yuvaIndices,
                                             shrunkImageSize,
                                             kTopLeft_GrSurfaceOrigin);
    }

    void createImages(GrContext* context) {
        int counter = 0;
        for (bool opaque : { false, true }) {
            for (int cs = kJPEG_SkYUVColorSpace; cs <= kLastEnum_SkYUVColorSpace; ++cs) {
                PlaneData planes;
                extract_planes(fOriginalBMs[opaque], (SkYUVColorSpace) cs, &planes);

                for (int f = kP016_YUVFormat; f <= kLast_YUVFormat; ++f) {
                    auto format = static_cast<YUVFormat>(f);
                    SkBitmap resultBMs[4];

                    int numTextures = create_YUV(planes, format, resultBMs, opaque);

                    if (context) {
                        if (context->abandoned()) {
                            return;
                        }

                        if (!is_format_natively_supported(context, format)) {
                            continue;
                        }

                        GrBackendTexture yuvaTextures[4];
                        SkPixmap yuvaPixmaps[4];

                        for (int i = 0; i < numTextures; ++i) {
                            yuvaTextures[i] = create_yuva_texture(context, resultBMs[i]);
                            if (yuvaTextures[i].isValid()) {
                                fBackendTextures.push_back(yuvaTextures[i]);
                            }
                            yuvaPixmaps[i] = resultBMs[i].pixmap();
                        }

                        SkYUVAIndex yuvaIndices[4];
                        const auto& planarConfig = YUVAFormatPlanarConfig(format);
                        bool externalAlphaPlane = !opaque && !planarConfig.hasAlpha();
                        if (!planarConfig.getYUVAIndices(yuvaTextures, numTextures,
                                                         externalAlphaPlane, yuvaIndices)) {
                            continue;
                        }

                        if (fQuarterSize) {
                            fImages[opaque][cs][format] =
                                    this->resizeOnGpu(context,
                                                      format,
                                                      (SkYUVColorSpace)cs,
                                                      opaque,
                                                      yuvaTextures,
                                                      yuvaIndices,
                                                      numTextures,
                                                      fOriginalBMs[opaque].dimensions());
                        } else {
                            int counterMod = counter % 3;
                            if (fUseDomain && counterMod == 0) {
                                // Copies flatten to RGB when they copy the YUVA data, which doesn't
                                // know about the intended domain and the domain padding bleeds in
                                counterMod = 1;
                            }

                            switch (counterMod) {
                            case 0:
                                fImages[opaque][cs][format] = SkImage::MakeFromYUVATexturesCopy(
                                    context,
                                    (SkYUVColorSpace)cs,
                                    yuvaTextures,
                                    yuvaIndices,
                                    { fOriginalBMs[opaque].width(), fOriginalBMs[opaque].height() },
                                    kTopLeft_GrSurfaceOrigin);
                                break;
                            case 1:
                                fImages[opaque][cs][format] = SkImage::MakeFromYUVATextures(
                                    context,
                                    (SkYUVColorSpace)cs,
                                    yuvaTextures,
                                    yuvaIndices,
                                    { fOriginalBMs[opaque].width(), fOriginalBMs[opaque].height() },
                                    kTopLeft_GrSurfaceOrigin);
                                break;
                            case 2:
                            default:
                                fImages[opaque][cs][format] = SkImage::MakeFromYUVAPixmaps(
                                    context,
                                    (SkYUVColorSpace)cs,
                                    yuvaPixmaps,
                                    yuvaIndices,
                                    { fOriginalBMs[opaque].width(), fOriginalBMs[opaque].height() },
                                    kTopLeft_GrSurfaceOrigin, true);
                                break;
                            }
                            ++counter;
                        }
                    } else {
                        SkImageInfo ii = SkImageInfo::MakeN32(fOriginalBMs[opaque].width(),
                                                              fOriginalBMs[opaque].height(),
                                                              kPremul_SkAlphaType);
                        fImages[opaque][cs][format] = make_yuv_gen_image(
                                ii, format, (SkYUVColorSpace)cs, opaque, resultBMs);
                    }
                }
            }
        }
    }

    void onDraw(SkCanvas* canvas) override {
        this->createImages(canvas->getGrContext());

        float cellWidth = kTileWidthHeight, cellHeight = kTileWidthHeight;
        if (fUseDomain) {
            cellWidth *= 1.5f;
            cellHeight *= 1.5f;
        }

        SkRect origSrcRect = SkRect::MakeWH(fOriginalBMs[0].width(), fOriginalBMs[0].height());

        SkRect srcRect = SkRect::MakeWH(fOriginalBMs[0].width(), fOriginalBMs[0].height());
        SkRect dstRect = SkRect::MakeXYWH(kLabelWidth, 0.f, srcRect.width(), srcRect.height());
        if (fQuarterSize) {
            if (canvas->getGrContext()) {
                // The src is only shrunk on the GPU
                srcRect = SkRect::MakeWH(fOriginalBMs[0].width()/2.0f,
                                         fOriginalBMs[0].height()/2.0f);
            }
            // but the dest is always drawn smaller
            dstRect = SkRect::MakeXYWH(kLabelWidth, 0.f,
                                       fOriginalBMs[0].width()/2.0f,
                                       fOriginalBMs[0].height()/2.0f);
        }

        SkCanvas::SrcRectConstraint constraint = SkCanvas::kFast_SrcRectConstraint;
        if (fUseDomain) {
            srcRect.inset(kDomainPadding, kDomainPadding);
            origSrcRect.inset(kDomainPadding, kDomainPadding);
            // Draw a larger rectangle to ensure bilerp filtering would normally read outside the
            // srcRect and hit the red pixels, if strict constraint weren't used.
            dstRect.fRight = kLabelWidth + 1.5f * srcRect.width();
            dstRect.fBottom = 1.5f * srcRect.height();
            constraint = SkCanvas::kStrict_SrcRectConstraint;
        }

        for (int cs = kJPEG_SkYUVColorSpace; cs <= kLastEnum_SkYUVColorSpace; ++cs) {
            SkPaint paint;
            paint.setFilterQuality(kLow_SkFilterQuality);
            if (kIdentity_SkYUVColorSpace == cs) {
                // The identity color space needs post processing to appear correctly
                paint.setColorFilter(yuv_to_rgb_colorfilter());
            }

            for (int opaque : { 0, 1 }) {
                dstRect.offsetTo(dstRect.fLeft, kLabelHeight);

                draw_col_label(canvas, dstRect.fLeft + cellWidth / 2, cs, opaque);

                canvas->drawBitmapRect(fOriginalBMs[opaque], origSrcRect, dstRect,
                                       nullptr, constraint);
                dstRect.offset(0.f, cellHeight + kPad);

                for (int format = kP016_YUVFormat; format <= kLast_YUVFormat; ++format) {
                    draw_row_label(canvas, dstRect.fTop, format);
                    if (fUseTargetColorSpace && fImages[opaque][cs][format]) {
                        // Making a CS-specific version of a kIdentity_SkYUVColorSpace YUV image
                        // doesn't make a whole lot of sense. The colorSpace conversion will
                        // operate on the YUV components rather than the RGB components.
                        sk_sp<SkImage> csImage =
                            fImages[opaque][cs][format]->makeColorSpace(fTargetColorSpace);
                        canvas->drawImageRect(csImage, srcRect, dstRect, &paint, constraint);
                    } else {
                        canvas->drawImageRect(fImages[opaque][cs][format], srcRect, dstRect,
                                              &paint, constraint);
                    }
                    dstRect.offset(0.f, cellHeight + kPad);
                }

                dstRect.offset(cellWidth + kPad, 0.f);
            }
        }
        if (auto context = canvas->getGrContext()) {
            if (!context->abandoned()) {
                context->flush();
                GrGpu* gpu = context->priv().getGpu();
                SkASSERT(gpu);
                gpu->testingOnly_flushGpuAndSync();
                for (const auto& tex : fBackendTextures) {
                    context->deleteBackendTexture(tex);
                }
                fBackendTextures.reset();
            }
        }
        SkASSERT(!fBackendTextures.count());
    }

private:
    SkBitmap                   fOriginalBMs[2];
    sk_sp<SkImage>             fImages[2][kLastEnum_SkYUVColorSpace + 1][kLast_YUVFormat + 1];
    SkTArray<GrBackendTexture> fBackendTextures;
    bool                       fUseTargetColorSpace;
    bool                       fUseDomain;
    bool                       fQuarterSize;
    sk_sp<SkColorSpace>        fTargetColorSpace;

    typedef GM INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

DEF_GM(return new WackyYUVFormatsGM(/* cs */ false, /* domain */ false, /* quarterSize */ false);)
DEF_GM(return new WackyYUVFormatsGM(/* cs */ false, /* domain */ false, /* quarterSize */ true);)
DEF_GM(return new WackyYUVFormatsGM(/* cs */ true,  /* domain */ false, /* quarterSize */ false);)
DEF_GM(return new WackyYUVFormatsGM(/* cs */ false, /* domain */ true,  /* quarterSize */ false);)

class YUVMakeColorSpaceGM : public GpuGM {
public:
    YUVMakeColorSpaceGM() {
        this->setBGColor(0xFFCCCCCC);
    }

protected:
    SkString onShortName() override {
        return SkString("yuv_make_color_space");
    }

    SkISize onISize() override {
        int numCols = 4; // (transparent, opaque) x (untagged, tagged)
        int numRows = 5; // original, YUV, subset, readPixels, makeNonTextureImage
        return SkISize::Make(numCols * (kTileWidthHeight + kPad) + kPad,
                             numRows * (kTileWidthHeight + kPad) + kPad);
    }

    void onOnceBeforeDraw() override {
        SkPoint origin = { kTileWidthHeight/2.0f, kTileWidthHeight/2.0f };
        float outerRadius = kTileWidthHeight/2.0f - 20.0f;
        float innerRadius = 20.0f;

        {
            // transparent
            SkTDArray<SkRect> circles;
            SkPath path = create_splat(origin, innerRadius, outerRadius, 1.0f, 5, &circles);
            fOriginalBMs[0] = make_bitmap(kN32_SkColorType, path, circles, false, false);
        }

        {
            // opaque
            SkTDArray<SkRect> circles;
            SkPath path = create_splat(origin, innerRadius, outerRadius, 1.0f, 7, &circles);
            fOriginalBMs[1] = make_bitmap(kN32_SkColorType, path, circles, true, false);
        }

        fTargetColorSpace = SkColorSpace::MakeSRGB()->makeColorSpin();
    }

    void createImages(GrContext* context) {
        for (bool opaque : { false, true }) {
            PlaneData planes;
            extract_planes(fOriginalBMs[opaque], kJPEG_SkYUVColorSpace, &planes);

            SkBitmap resultBMs[4];

            create_YUV(planes, kAYUV_YUVFormat, resultBMs, opaque);

            auto& planarConfig = YUVAFormatPlanarConfig(kAYUV_YUVFormat);
            int numPlanes = planarConfig.numPlanes();

            GrBackendTexture yuvaTextures[4];
            for (int i = 0; i < numPlanes; ++i) {
                yuvaTextures[i] = create_yuva_texture(context, resultBMs[i]);
                if (yuvaTextures[i].isValid()) {
                    fBackendTextures.push_back(yuvaTextures[i]);
                }
            }

            SkYUVAIndex yuvaIndices[4];
            planarConfig.getYUVAIndices(yuvaTextures, numPlanes, false, yuvaIndices);
            fImages[opaque][0] = SkImage::MakeFromYUVATextures(
                    context,
                    kJPEG_SkYUVColorSpace,
                    yuvaTextures,
                    yuvaIndices,
                    { fOriginalBMs[opaque].width(), fOriginalBMs[opaque].height() },
                    kTopLeft_GrSurfaceOrigin);
            fImages[opaque][1] = SkImage::MakeFromYUVATextures(
                    context,
                    kJPEG_SkYUVColorSpace,
                    yuvaTextures,
                    yuvaIndices,
                    { fOriginalBMs[opaque].width(), fOriginalBMs[opaque].height() },
                    kTopLeft_GrSurfaceOrigin,
                    SkColorSpace::MakeSRGB());
        }
    }

    void onDraw(GrContext* context, GrRenderTargetContext*, SkCanvas* canvas) override {
        this->createImages(context);

        int x = kPad;
        for (int tagged : { 0, 1 }) {
            for (int opaque : { 0, 1 }) {
                int y = kPad;

                auto raster = SkImage::MakeFromBitmap(fOriginalBMs[opaque])
                    ->makeColorSpace(fTargetColorSpace);
                canvas->drawImage(raster, x, y);
                y += kTileWidthHeight + kPad;

                if (fImages[opaque][tagged]) {
                    auto yuv = fImages[opaque][tagged]->makeColorSpace(fTargetColorSpace);
                    SkASSERT(SkColorSpace::Equals(yuv->colorSpace(), fTargetColorSpace.get()));
                    canvas->drawImage(yuv, x, y);
                    y += kTileWidthHeight + kPad;

                    auto subset = yuv->makeSubset(SkIRect::MakeWH(kTileWidthHeight / 2,
                        kTileWidthHeight / 2));
                    canvas->drawImage(subset, x, y);
                    y += kTileWidthHeight + kPad;

                    auto nonTexture = yuv->makeNonTextureImage();
                    canvas->drawImage(nonTexture, x, y);
                    y += kTileWidthHeight + kPad;

                    SkBitmap readBack;
                    readBack.allocPixels(yuv->imageInfo());
                    yuv->readPixels(readBack.pixmap(), 0, 0);
                    canvas->drawBitmap(readBack, x, y);
                }
                x += kTileWidthHeight + kPad;
            }
        }

        context->flush();
        GrGpu* gpu = context->priv().getGpu();
        SkASSERT(gpu);
        gpu->testingOnly_flushGpuAndSync();
        for (const auto& tex : fBackendTextures) {
            context->deleteBackendTexture(tex);
        }
        fBackendTextures.reset();
    }

private:
    SkBitmap fOriginalBMs[2];
    sk_sp<SkImage> fImages[2][2];
    SkTArray<GrBackendTexture> fBackendTextures;
    sk_sp<SkColorSpace> fTargetColorSpace;

    typedef GM INHERITED;
};

DEF_GM(return new YUVMakeColorSpaceGM();)

}

///////////////

#include "include/effects/SkColorMatrix.h"
#include "src/core/SkAutoPixmapStorage.h"
#include "tools/Resources.h"

static void draw_into_alpha(const SkImage* img, sk_sp<SkColorFilter> cf, const SkPixmap& dst) {
    auto canvas = SkCanvas::MakeRasterDirect(dst.info(), dst.writable_addr(), dst.rowBytes());
    canvas->scale(1.0f * dst.width() / img->width(), 1.0f * dst.height() / img->height());
    SkPaint paint;
    paint.setFilterQuality(kLow_SkFilterQuality);
    paint.setColorFilter(cf);
    paint.setBlendMode(SkBlendMode::kSrc);
    canvas->drawImage(img, 0, 0, &paint);
}

static void split_into_yuv(const SkImage* img, SkYUVColorSpace cs, const SkPixmap dst[3]) {
    float m[20];
    SkColorMatrix_RGB2YUV(cs, m);

    memcpy(m + 15, m + 0, 5 * sizeof(float));   // copy Y into A
    draw_into_alpha(img, SkColorFilters::Matrix(m), dst[0]);

    memcpy(m + 15, m + 5, 5 * sizeof(float));   // copy U into A
    draw_into_alpha(img, SkColorFilters::Matrix(m), dst[1]);

    memcpy(m + 15, m + 10, 5 * sizeof(float));   // copy V into A
    draw_into_alpha(img, SkColorFilters::Matrix(m), dst[2]);
}

static void draw_diff(SkCanvas* canvas, SkScalar x, SkScalar y,
                      const SkImage* a, const SkImage* b) {
    auto sh = SkShaders::Blend(SkBlendMode::kDifference, a->makeShader(), b->makeShader());
    SkPaint paint;
    paint.setShader(sh);
    canvas->save();
    canvas->translate(x, y);
    canvas->drawRect(SkRect::MakeWH(a->width(), a->height()), paint);

    SkColorMatrix cm;
    cm.setScale(64, 64, 64);
    paint.setShader(sh->makeWithColorFilter(SkColorFilters::Matrix(cm)));
    canvas->translate(0, a->height());
    canvas->drawRect(SkRect::MakeWH(a->width(), a->height()), paint);

    canvas->restore();
}

// Exercises SkColorMatrix_RGB2YUV for yuv colorspaces, showing the planes, and the
// resulting (recombined) images (gpu only for now).
//
class YUVSplitterGM : public skiagm::GM {
    sk_sp<SkImage>      fOrig;
    SkAutoPixmapStorage fStorage[3];
    SkPixmap            fPM[3];

public:
    YUVSplitterGM() {}

protected:

    SkString onShortName() override {
        return SkString("yuv_splitter");
    }

    SkISize onISize() override {
        return SkISize::Make(1280, 768);
    }

    void onOnceBeforeDraw() override {
        fOrig = GetResourceAsImage("images/mandrill_256.png");

        SkImageInfo info = SkImageInfo::Make(fOrig->width(), fOrig->height(), kAlpha_8_SkColorType,
                                             kPremul_SkAlphaType);
        fStorage[0].alloc(info);
        if (0) {
            // if you want to scale U,V down by 1/2
            info = info.makeWH(info.width()/2, info.height()/2);
        }
        fStorage[1].alloc(info);
        fStorage[2].alloc(info);
        for (int i = 0; i < 3; ++i) {
            fPM[i] = fStorage[i];
        }
    }

    void onDraw(SkCanvas* canvas) override {
        SkYUVAIndex indices[4];
        indices[SkYUVAIndex::kY_Index] = {0,  SkColorChannel::kR};
        indices[SkYUVAIndex::kU_Index] = {1,  SkColorChannel::kR};
        indices[SkYUVAIndex::kV_Index] = {2,  SkColorChannel::kR};
        indices[SkYUVAIndex::kA_Index] = {-1, SkColorChannel::kR};

        canvas->translate(fOrig->width(), 0);
        canvas->save();
        for (auto cs : {kRec709_SkYUVColorSpace, kRec601_SkYUVColorSpace, kJPEG_SkYUVColorSpace,
                        kBT2020_SkYUVColorSpace}) {
            split_into_yuv(fOrig.get(), cs, fPM);
            auto img = SkImage::MakeFromYUVAPixmaps(canvas->getGrContext(), cs, fPM, indices,
                                                    fPM[0].info().dimensions(),
                                                    kTopLeft_GrSurfaceOrigin,
                                                    false, false, nullptr);
            if (img) {
                canvas->drawImage(img, 0, 0, nullptr);
                draw_diff(canvas, 0, fOrig->height(), fOrig.get(), img.get());
            }
            canvas->translate(fOrig->width(), 0);
        }
        canvas->restore();
        canvas->translate(-fOrig->width(), 0);

        canvas->drawImage(SkImage::MakeRasterCopy(fPM[0]), 0, 0, nullptr);
        canvas->drawImage(SkImage::MakeRasterCopy(fPM[1]), 0, fPM[0].height(), nullptr);
        canvas->drawImage(SkImage::MakeRasterCopy(fPM[2]),
                          0, fPM[0].height() + fPM[1].height(), nullptr);
    }

private:
    typedef GM INHERITED;
};
DEF_GM( return new YUVSplitterGM; )
