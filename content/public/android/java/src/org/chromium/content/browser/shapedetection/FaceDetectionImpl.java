// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.shapedetection;

import android.graphics.Bitmap;
import android.graphics.PointF;
import android.media.FaceDetector;
import android.media.FaceDetector.Face;

import org.chromium.base.Log;
import org.chromium.blink.mojom.FaceDetection;
import org.chromium.blink.mojom.FaceDetectionResult;
import org.chromium.blink.mojom.FaceDetectorOptions;
import org.chromium.gfx.mojom.RectF;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.SharedBufferHandle;
import org.chromium.mojo.system.SharedBufferHandle.MapFlags;

import java.nio.ByteBuffer;

/**
 * Android implementation of the FaceDetection service defined in
 * third_party/WebKit/public/platform/modules/FaceDetection/FaceDetection.mojom
 */
public class FaceDetectionImpl implements FaceDetection {
    private static final String TAG = "FaceDetectionImpl";
    // By default, there is no limit in the number of faces detected.
    private static final int MAX_FACES = 10;
    // Referred from
    // https://cs.chromium.org/chromium/src/mojo/edk/system/broker_host.cc?l=24
    private static final int MOJO_SHAREDBUFFER_MAX_BYTES = 16 * 1024 * 1024;

    @Override
    public void detect(SharedBufferHandle frameData, int width, int height,
            FaceDetectorOptions options, DetectResponse callback) {
        if (!frameData.isValid()) {
            Log.d(TAG, "Invalid sharedBufferHandle.");
            return;
        }
        if (width <= 0 || height <= 0) {
            Log.d(TAG, "Invalid image width or height.");
            return;
        }

        final long numPixels = (long) width * height;
        if (numPixels > MOJO_SHAREDBUFFER_MAX_BYTES / 4) {
            Log.d(TAG, "Data size exceeds the limit of mojo::SharedBufferHandle.");
            return;
        }

        ByteBuffer imageBuffer = frameData.map(0, numPixels * 4, MapFlags.none());
        if (imageBuffer.capacity() <= 0) {
            Log.d(TAG, "Failed to map from SharedBufferHandle.");
            return;
        }

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);

        // An int array is needed to construct a Bitmap. However the Bytebuffer
        // we get from |sharedBufferHandle| is directly allocated and does not
        // have a supporting array. Therefore we need to copy from |imageBuffer|
        // to create this intermediate Bitmap.
        // TODO(xianglu): Consider worker pool as appropriate threads.
        // http://crbug.com/655814
        bitmap.copyPixelsFromBuffer(imageBuffer);

        // A Bitmap must be in 565 format for findFaces() to work. See
        // http://androidxref.com/7.0.0_r1/xref/frameworks/base/media/java/android/media/FaceDetector.java#124
        //
        // It turns out that FaceDetector is not able to detect correctly if
        // simply using pixmap.setConfig(). The reason might be that findFaces()
        // needs non-premultiplied ARGB arrangement, while the alpha type in the
        // original image is premultiplied. We can use getPixels() which does
        // the unmultiplication while copying to a new array. See
        // http://androidxref.com/7.0.0_r1/xref/frameworks/base/graphics/java/android/graphics/Bitmap.java#538
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        Bitmap unPremultipliedBitmap =
                Bitmap.createBitmap(pixels, width, height, Bitmap.Config.RGB_565);

        FaceDetector detector = new FaceDetector(width, height, MAX_FACES);
        Face[] detectedFaces = new Face[MAX_FACES];
        // findFaces() will stop at MAX_FACES.
        final int numberOfFaces = detector.findFaces(unPremultipliedBitmap, detectedFaces);

        FaceDetectionResult faceDetectionResult = new FaceDetectionResult();
        faceDetectionResult.boundingBoxes = new RectF[numberOfFaces];
        for (int i = 0; i < numberOfFaces; i++) {
            final Face face = detectedFaces[i];
            final PointF midPoint = new PointF();
            face.getMidPoint(midPoint);
            final float eyesDistance = face.eyesDistance();

            RectF boundingBox = new RectF();
            boundingBox.x = midPoint.x - eyesDistance;
            boundingBox.y = midPoint.y - eyesDistance;
            boundingBox.width = 2 * eyesDistance;
            boundingBox.height = 2 * eyesDistance;

            // TODO(xianglu): Consider adding Face.confidence and Face.pose.
            faceDetectionResult.boundingBoxes[i] = boundingBox;
        }

        callback.call(faceDetectionResult);
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }
}
