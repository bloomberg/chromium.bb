// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.os.ParcelFileDescriptor;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.File;
import java.nio.ByteBuffer;

@JNINamespace("media")
class WebAudioMediaCodecBridge {
    private static final String TAG = "cr.media";
    // TODO(rtoy): What is the correct timeout value for reading
    // from a file in memory?
    static final long TIMEOUT_MICROSECONDS = 500;
    @CalledByNative
    private static String createTempFile(Context ctx) throws java.io.IOException {
        File outputDirectory = ctx.getCacheDir();
        File outputFile = File.createTempFile("webaudio", ".dat", outputDirectory);
        return outputFile.getAbsolutePath();
    }

    @SuppressWarnings("deprecation")
    @CalledByNative
    private static boolean decodeAudioFile(Context ctx, long nativeMediaCodecBridge,
            int inputFD, long dataSize) {

        if (dataSize < 0 || dataSize > 0x7fffffff) return false;

        MediaExtractor extractor = new MediaExtractor();

        ParcelFileDescriptor encodedFD;
        encodedFD = ParcelFileDescriptor.adoptFd(inputFD);
        try {
            extractor.setDataSource(encodedFD.getFileDescriptor(), 0, dataSize);
        } catch (Exception e) {
            e.printStackTrace();
            encodedFD.detachFd();
            return false;
        }

        if (extractor.getTrackCount() <= 0) {
            encodedFD.detachFd();
            return false;
        }

        MediaFormat format = extractor.getTrackFormat(0);

        // If we are unable to get the input channel count, the sample
        // rate or the mime type for any reason, just give up.
        // Without these, we don't know what to do.

        int inputChannelCount;
        try {
            // Number of channels specified in the file
            inputChannelCount = format.getInteger(MediaFormat.KEY_CHANNEL_COUNT);
        } catch (Exception e) {
            // Give up.
            Log.w(TAG, "Unable to determine number of channels");
            encodedFD.detachFd();
            return false;
        }

        // Number of channels the decoder will provide. (Not
        // necessarily the same as inputChannelCount.  See
        // crbug.com/266006.)
        int outputChannelCount = inputChannelCount;

        int sampleRate;
        try {
            sampleRate = format.getInteger(MediaFormat.KEY_SAMPLE_RATE);
        } catch (Exception e) {
            // Give up.
            Log.w(TAG, "Unable to determine sample rate");
            encodedFD.detachFd();
            return false;
        }

        String mime;
        try {
            mime = format.getString(MediaFormat.KEY_MIME);
        } catch (Exception e) {
            // Give up.
            Log.w(TAG, "Unable to determine type of encoding used by the file");
            encodedFD.detachFd();
            return false;
        }

        long durationMicroseconds = 0;
        if (format.containsKey(MediaFormat.KEY_DURATION)) {
            try {
                durationMicroseconds = format.getLong(MediaFormat.KEY_DURATION);
            } catch (Exception e) {
                Log.d(TAG, "Cannot get duration");
            }
        }

        // If the duration is too long, set to 0 to force the caller
        // not to preallocate space.  See crbug.com/326856.
        // FIXME: What should be the limit? We're arbitrarily using
        // about 2148 sec (35.8 min).
        if (durationMicroseconds > 0x7fffffff) {
            durationMicroseconds = 0;
        }

        Log.d(TAG, "Initial: Tracks: %d Format: %s", extractor.getTrackCount(), format);

        // Create decoder
        MediaCodec codec;
        try {
            codec = MediaCodec.createDecoderByType(mime);
        } catch (Exception e) {
            Log.w(TAG, "Failed to create MediaCodec for mime type: %s", mime);
            encodedFD.detachFd();
            return false;
        }

        try {
            codec.configure(format, null /* surface */, null /* crypto */, 0 /* flags */);
        } catch (Exception e) {
            Log.w(TAG, "Unable to configure codec for format " + format, e);
            encodedFD.detachFd();
            return false;
        }
        try {
            codec.start();
        } catch (Exception e) {
            Log.w(TAG, "Unable to start()", e);
            encodedFD.detachFd();
            return false;
        }

        ByteBuffer[] codecInputBuffers;
        try {
            codecInputBuffers = codec.getInputBuffers();
        } catch (Exception e) {
            Log.w(TAG, "getInputBuffers() failed", e);
            encodedFD.detachFd();
            return false;
        }
        ByteBuffer[] codecOutputBuffers;
        try {
            codecOutputBuffers = codec.getOutputBuffers();
        } catch (Exception e) {
            Log.w(TAG, "getOutputBuffers() failed", e);
            encodedFD.detachFd();
            return false;
        }

        // A track must be selected and will be used to read samples.
        extractor.selectTrack(0);

        boolean sawInputEOS = false;
        boolean sawOutputEOS = false;
        boolean destinationInitialized = false;
        boolean decodedSuccessfully = true;

        // Keep processing until the output is done.
        while (!sawOutputEOS) {
            if (!sawInputEOS) {
                // Input side
                int inputBufIndex;
                try {
                    inputBufIndex = codec.dequeueInputBuffer(TIMEOUT_MICROSECONDS);
                } catch (Exception e) {
                    Log.w(TAG, "dequeueInputBuffer(%d) failed.", TIMEOUT_MICROSECONDS, e);
                    decodedSuccessfully = false;
                    break;
                }

                if (inputBufIndex >= 0) {
                    ByteBuffer dstBuf = codecInputBuffers[inputBufIndex];
                    int sampleSize;

                    try {
                        sampleSize = extractor.readSampleData(dstBuf, 0);
                    } catch (Exception e) {
                        Log.w(TAG, "readSampleData failed.");
                        decodedSuccessfully = false;
                        break;
                    }

                    long presentationTimeMicroSec = 0;

                    if (sampleSize < 0) {
                        sawInputEOS = true;
                        sampleSize = 0;
                    } else {
                        presentationTimeMicroSec = extractor.getSampleTime();
                    }

                    try {
                        codec.queueInputBuffer(inputBufIndex,
                                0, /* offset */
                                sampleSize,
                                presentationTimeMicroSec,
                                sawInputEOS ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0);
                    } catch (Exception e) {
                        Log.w(TAG, "queueInputBuffer(%d, 0, %d, %d, %d) failed.",
                                inputBufIndex, sampleSize, presentationTimeMicroSec,
                                (sawInputEOS ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0), e);
                        decodedSuccessfully = false;
                        break;
                    }

                    if (!sawInputEOS) {
                        extractor.advance();
                    }
                }
            }

            // Output side
            MediaCodec.BufferInfo info = new BufferInfo();
            final int outputBufIndex;

            try {
                outputBufIndex = codec.dequeueOutputBuffer(info, TIMEOUT_MICROSECONDS);
            } catch (Exception e) {
                Log.w(TAG, "dequeueOutputBuffer(%s, %d) failed", info, TIMEOUT_MICROSECONDS);
                e.printStackTrace();
                decodedSuccessfully = false;
                break;
            }

            if (outputBufIndex >= 0) {
                ByteBuffer buf = codecOutputBuffers[outputBufIndex];

                if (!destinationInitialized) {
                    // Initialize the destination as late as possible to
                    // catch any changes in format. But be sure to
                    // initialize it BEFORE we send any decoded audio,
                    // and only initialize once.
                    Log.d(TAG, "Final:  Rate: %d Channels: %d Mime: %s Duration: %d microsec",
                            sampleRate, inputChannelCount, mime, durationMicroseconds);

                    nativeInitializeDestination(nativeMediaCodecBridge,
                                                inputChannelCount,
                                                sampleRate,
                                                durationMicroseconds);
                    destinationInitialized = true;
                }

                if (destinationInitialized && info.size > 0) {
                    nativeOnChunkDecoded(nativeMediaCodecBridge, buf, info.size,
                                         inputChannelCount, outputChannelCount);
                }

                buf.clear();
                codec.releaseOutputBuffer(outputBufIndex, false /* render */);

                if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                    sawOutputEOS = true;
                }
            } else if (outputBufIndex == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                codecOutputBuffers = codec.getOutputBuffers();
            } else if (outputBufIndex == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                MediaFormat newFormat = codec.getOutputFormat();
                outputChannelCount = newFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT);
                sampleRate = newFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE);
                Log.d(TAG, "output format changed to " + newFormat);
            }
        }

        encodedFD.detachFd();

        codec.stop();
        codec.release();
        codec = null;

        return decodedSuccessfully;
    }

    private static native void nativeOnChunkDecoded(
            long nativeWebAudioMediaCodecBridge, ByteBuffer buf, int size,
            int inputChannelCount, int outputChannelCount);

    private static native void nativeInitializeDestination(
            long nativeWebAudioMediaCodecBridge,
            int inputChannelCount,
            int sampleRate,
            long durationMicroseconds);
}
