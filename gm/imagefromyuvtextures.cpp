/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This test only works with the GPU backend.

#include "gm.h"

#include "GrBackendSurface.h"
#include "GrContext.h"
#include "GrContextPriv.h"
#include "GrGpu.h"
#include "SkBitmap.h"
#include "SkGradientShader.h"
#include "SkImage.h"
#include "SkTo.h"

namespace skiagm {
class ImageFromYUVTextures : public GpuGM {
public:
    ImageFromYUVTextures() {
        this->setBGColor(0xFFFFFFFF);
    }

protected:
    SkString onShortName() override {
        return SkString("image_from_yuv_textures");
    }

    SkISize onISize() override {
        return SkISize::Make(50, 300);
    }

    void onOnceBeforeDraw() override {
        // We create an RGB bitmap and then extract YUV bmps where the U and V bitmaps are
        // subsampled by 2 in both dimensions.
        SkPaint paint;
        constexpr SkColor kColors[] =
            { SK_ColorBLUE, SK_ColorYELLOW, SK_ColorGREEN, SK_ColorWHITE };
        paint.setShader(SkGradientShader::MakeRadial(SkPoint::Make(0,0), kBmpSize / 2.f, kColors,
                                                     nullptr, SK_ARRAY_COUNT(kColors),
                                                     SkShader::kMirror_TileMode));
        SkBitmap rgbBmp;
        rgbBmp.allocN32Pixels(kBmpSize, kBmpSize, true);
        SkCanvas canvas(rgbBmp);
        canvas.drawPaint(paint);
        SkPMColor* rgbColors = static_cast<SkPMColor*>(rgbBmp.getPixels());

        SkImageInfo yinfo = SkImageInfo::MakeA8(kBmpSize, kBmpSize);
        fYUVBmps[0].allocPixels(yinfo);
        SkImageInfo uinfo = SkImageInfo::MakeA8(kBmpSize / 2, kBmpSize / 2);
        fYUVBmps[1].allocPixels(uinfo);
        SkImageInfo vinfo = SkImageInfo::MakeA8(kBmpSize / 2, kBmpSize / 2);
        fYUVBmps[2].allocPixels(vinfo);
        unsigned char* yPixels;
        signed char* uvPixels[2];
        yPixels = static_cast<unsigned char*>(fYUVBmps[0].getPixels());
        uvPixels[0] = static_cast<signed char*>(fYUVBmps[1].getPixels());
        uvPixels[1] = static_cast<signed char*>(fYUVBmps[2].getPixels());

        // Here we encode using the NTC encoding (even though we will draw it with all the supported
        // yuv color spaces when converted back to RGB)
        for (int i = 0; i < kBmpSize * kBmpSize; ++i) {
            yPixels[i] = static_cast<unsigned char>(0.299f * SkGetPackedR32(rgbColors[i]) +
                                                    0.587f * SkGetPackedG32(rgbColors[i]) +
                                                    0.114f * SkGetPackedB32(rgbColors[i]));
        }
        for (int j = 0; j < kBmpSize / 2; ++j) {
            for (int i = 0; i < kBmpSize / 2; ++i) {
                // Average together 4 pixels of RGB.
                int rgb[] = { 0, 0, 0 };
                for (int y = 0; y < 2; ++y) {
                    for (int x = 0; x < 2; ++x) {
                        int rgbIndex = (2 * j + y) * kBmpSize + 2 * i + x;
                        rgb[0] += SkGetPackedR32(rgbColors[rgbIndex]);
                        rgb[1] += SkGetPackedG32(rgbColors[rgbIndex]);
                        rgb[2] += SkGetPackedB32(rgbColors[rgbIndex]);
                    }
                }
                for (int c = 0; c < 3; ++c) {
                    rgb[c] /= 4;
                }
                int uvIndex = j * kBmpSize / 2 + i;
                uvPixels[0][uvIndex] = static_cast<signed char>(
                    ((-38 * rgb[0] -  74 * rgb[1] + 112 * rgb[2] + 128) >> 8) + 128);
                uvPixels[1][uvIndex] = static_cast<signed char>(
                    ((112 * rgb[0] -  94 * rgb[1] -  18 * rgb[2] + 128) >> 8) + 128);
            }
        }
        fRGBImage = SkImage::MakeRasterCopy(SkPixmap(rgbBmp.info(), rgbColors, rgbBmp.rowBytes()));
    }

    void createYUVTextures(GrContext* context, GrBackendTexture yuvTextures[3]) {
        GrGpu* gpu = context->priv().getGpu();
        if (!gpu) {
            return;
        }

        for (int i = 0; i < 3; ++i) {
            SkASSERT(fYUVBmps[i].width() == SkToInt(fYUVBmps[i].rowBytes()));
            yuvTextures[i] = gpu->createTestingOnlyBackendTexture(fYUVBmps[i].getPixels(),
                                                                  fYUVBmps[i].width(),
                                                                  fYUVBmps[i].height(),
                                                                  GrColorType::kAlpha_8,
                                                                  false, GrMipMapped::kNo);
        }
        context->resetContext();
    }

    void createResultTexture(GrContext* context, int width, int height,
                             GrBackendTexture* resultTexture) {
        GrGpu* gpu = context->priv().getGpu();
        if (!gpu) {
            return;
        }

        *resultTexture = gpu->createTestingOnlyBackendTexture(
                nullptr, width, height, GrColorType::kRGBA_8888, true, GrMipMapped::kNo);

        context->resetContext();
    }

    void deleteBackendTextures(GrContext* context, GrBackendTexture textures[], int n) {
        if (context->abandoned()) {
            return;
        }

        GrGpu* gpu = context->priv().getGpu();
        if (!gpu) {
            return;
        }

        context->flush();
        gpu->testingOnly_flushGpuAndSync();
        for (int i = 0; i < n; ++i) {
            if (textures[i].isValid()) {
                gpu->deleteTestingOnlyBackendTexture(textures[i]);
            }
        }

        context->resetContext();
    }

    void onDraw(GrContext* context, GrRenderTargetContext*, SkCanvas* canvas) override {
        constexpr SkScalar kPad = 10.f;

        SkTArray<sk_sp<SkImage>> images;
        images.push_back(fRGBImage);
        for (int space = kJPEG_SkYUVColorSpace; space <= kLastEnum_SkYUVColorSpace; ++space) {
            GrBackendTexture yuvTextures[3];
            this->createYUVTextures(context, yuvTextures);
            images.push_back(SkImage::MakeFromYUVTexturesCopy(context,
                                                              static_cast<SkYUVColorSpace>(space),
                                                              yuvTextures,
                                                              kTopLeft_GrSurfaceOrigin));
            this->deleteBackendTextures(context, yuvTextures, 3);
        }
        for (int i = 0; i < images.count(); ++ i) {
            SkScalar y = (i + 1) * kPad + i * fYUVBmps[0].height();
            SkScalar x = kPad;

            canvas->drawImage(images[i].get(), x, y);
        }

        sk_sp<SkImage> image;
        for (int space = kJPEG_SkYUVColorSpace, i = images.count();
             space <= kLastEnum_SkYUVColorSpace; ++space, ++i) {
            GrBackendTexture yuvTextures[3];
            GrBackendTexture resultTexture;
            this->createYUVTextures(context, yuvTextures);
            this->createResultTexture(
                    context, yuvTextures[0].width(), yuvTextures[0].height(), &resultTexture);
            image = SkImage::MakeFromYUVTexturesCopyWithExternalBackend(
                    context,
                    static_cast<SkYUVColorSpace>(space),
                    yuvTextures,
                    kTopLeft_GrSurfaceOrigin,
                    resultTexture);

            SkScalar y = (i + 1) * kPad + i * fYUVBmps[0].height();
            SkScalar x = kPad;

            canvas->drawImage(image.get(), x, y);
            GrBackendTexture texturesToDelete[4]{
                    yuvTextures[0],
                    yuvTextures[1],
                    yuvTextures[2],
                    resultTexture,
            };
            this->deleteBackendTextures(context, texturesToDelete, 4);
        }
     }

private:
    sk_sp<SkImage>  fRGBImage;
    SkBitmap        fYUVBmps[3];

    static constexpr int kBmpSize = 32;

    typedef GM INHERITED;
};

DEF_GM(return new ImageFromYUVTextures;)
}
