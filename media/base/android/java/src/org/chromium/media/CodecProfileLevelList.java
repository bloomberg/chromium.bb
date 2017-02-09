// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaCodecInfo.CodecProfileLevel;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;

import java.util.LinkedList;
import java.util.List;

@JNINamespace("media")
@MainDex
class CodecProfileLevelList {
    private static final String TAG = "CodecProfileLevelList";

    // The following values are taken from media/base/video_codecs.h. These need to be kept in sync.
    private static final int H264PROFILE_BASELINE = 0;
    private static final int H264PROFILE_MAIN = 1;
    private static final int H264PROFILE_EXTENDED = 2;
    private static final int H264PROFILE_HIGH = 3;
    private static final int H264PROFILE_HIGH10PROFILE = 4;
    private static final int H264PROFILE_HIGH422PROFILE = 5;
    private static final int H264PROFILE_HIGH444PREDICTIVEPROFILE = 6;
    private static final int H264PROFILE_SCALABLEBASELINE = 7;
    private static final int H264PROFILE_SCALABLEHIGH = 8;
    private static final int H264PROFILE_STEREOHIGH = 9;
    private static final int H264PROFILE_MULTIVIEWHIGH = 10;
    private static final int VP8PROFILE_ANY = 11;
    private static final int VP9PROFILE_PROFILE0 = 12;
    private static final int VP9PROFILE_PROFILE1 = 13;
    private static final int VP9PROFILE_PROFILE2 = 14;
    private static final int VP9PROFILE_PROFILE3 = 15;
    private static final int HEVCPROFILE_MAIN = 16;
    private static final int HEVCPROFILE_MAIN10 = 17;
    private static final int HEVCPROFILE_MAIN_STILL_PICTURE = 18;

    // Constants used to keep track of the codec from a mime string.
    private static final String CODEC_AVC = "AVC";
    private static final String CODEC_VP8 = "VP8";
    private static final String CODEC_VP9 = "VP9";
    private static final String CODEC_HEVC = "HEVC";

    private final List<CodecProfileLevelAdapter> mList;

    public CodecProfileLevelList() {
        mList = new LinkedList<CodecProfileLevelAdapter>();
    }

    public boolean addCodecProfileLevel(String mime, CodecProfileLevel codecProfileLevel) {
        try {
            String codec = getCodecFromMime(mime);
            mList.add(new CodecProfileLevelAdapter(codec,
                    mediaCodecProfileToChromiumMediaProfile(codec, codecProfileLevel.profile),
                    mediaCodecLevelToChromiumMediaLevel(codec, codecProfileLevel.level)));
            return true;
        } catch (UnsupportedCodecProfileException e) {
            return false;
        }
    }

    public boolean addCodecProfileLevel(String codec, int profile, int level) {
        mList.add(new CodecProfileLevelAdapter(codec, profile, level));
        return true;
    }

    public Object[] toArray() {
        return mList.toArray();
    }

    static class CodecProfileLevelAdapter {
        private final String mCodec;
        private final int mProfile;
        private final int mLevel;

        public CodecProfileLevelAdapter(String codec, int profile, int level) {
            mCodec = codec;
            mProfile = profile;
            mLevel = level;
        }

        @CalledByNative("CodecProfileLevelAdapter")
        public String getCodec() {
            return mCodec;
        }

        @CalledByNative("CodecProfileLevelAdapter")
        public int getProfile() {
            return mProfile;
        }

        @CalledByNative("CodecProfileLevelAdapter")
        public int getLevel() {
            return mLevel;
        }
    }

    private static class UnsupportedCodecProfileException extends RuntimeException {}

    private static String getCodecFromMime(String mime) {
        if (mime.endsWith("vp9")) return CODEC_VP9;
        if (mime.endsWith("vp8")) return CODEC_VP8;
        if (mime.endsWith("avc")) return CODEC_AVC;
        if (mime.endsWith("hevc")) return CODEC_HEVC;
        throw new UnsupportedCodecProfileException();
    }

    private static int mediaCodecProfileToChromiumMediaProfile(String codec, int profile) {
        // Note: The values returned here correspond to the VideoCodecProfile enum in
        // media/base/video_coedecs.h. These values need to be kept in sync with that file!
        switch (codec) {
            case CODEC_AVC:
                switch (profile) {
                    case CodecProfileLevel.AVCProfileBaseline:
                        return H264PROFILE_BASELINE;
                    case CodecProfileLevel.AVCProfileMain:
                        return H264PROFILE_MAIN;
                    case CodecProfileLevel.AVCProfileExtended:
                        return H264PROFILE_EXTENDED;
                    case CodecProfileLevel.AVCProfileHigh:
                        return H264PROFILE_HIGH;
                    case CodecProfileLevel.AVCProfileHigh10:
                        return H264PROFILE_HIGH10PROFILE;
                    case CodecProfileLevel.AVCProfileHigh422:
                        return H264PROFILE_HIGH422PROFILE;
                    case CodecProfileLevel.AVCProfileHigh444:
                        return H264PROFILE_HIGH444PREDICTIVEPROFILE;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case CODEC_VP8:
                switch (profile) {
                    case CodecProfileLevel.VP8ProfileMain:
                        return VP8PROFILE_ANY;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case CODEC_VP9:
                switch (profile) {
                    case CodecProfileLevel.VP9Profile0:
                        return VP9PROFILE_PROFILE0;
                    case CodecProfileLevel.VP9Profile1:
                        return VP9PROFILE_PROFILE1;
                    case CodecProfileLevel.VP9Profile2:
                        return VP9PROFILE_PROFILE2;
                    case CodecProfileLevel.VP9Profile3:
                        return VP9PROFILE_PROFILE3;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case CODEC_HEVC:
                switch (profile) {
                    case CodecProfileLevel.HEVCProfileMain:
                        return HEVCPROFILE_MAIN;
                    case CodecProfileLevel.HEVCProfileMain10:
                        return HEVCPROFILE_MAIN10;
                    case CodecProfileLevel.HEVCProfileMain10HDR10:
                        return HEVCPROFILE_MAIN_STILL_PICTURE;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            default:
                throw new UnsupportedCodecProfileException();
        }
    }

    private static int mediaCodecLevelToChromiumMediaLevel(String codec, int level) {
        switch (codec) {
            case CODEC_AVC:
                switch (level) {
                    case CodecProfileLevel.AVCLevel1:
                        return 10;
                    case CodecProfileLevel.AVCLevel11:
                        return 11;
                    case CodecProfileLevel.AVCLevel12:
                        return 12;
                    case CodecProfileLevel.AVCLevel13:
                        return 14;
                    case CodecProfileLevel.AVCLevel2:
                        return 20;
                    case CodecProfileLevel.AVCLevel21:
                        return 21;
                    case CodecProfileLevel.AVCLevel22:
                        return 22;
                    case CodecProfileLevel.AVCLevel3:
                        return 30;
                    case CodecProfileLevel.AVCLevel31:
                        return 31;
                    case CodecProfileLevel.AVCLevel32:
                        return 32;
                    case CodecProfileLevel.AVCLevel4:
                        return 40;
                    case CodecProfileLevel.AVCLevel41:
                        return 41;
                    case CodecProfileLevel.AVCLevel42:
                        return 42;
                    case CodecProfileLevel.AVCLevel5:
                        return 50;
                    case CodecProfileLevel.AVCLevel51:
                        return 51;
                    case CodecProfileLevel.AVCLevel52:
                        return 52;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case CODEC_VP8:
                switch (level) {
                    case CodecProfileLevel.VP8Level_Version0:
                        return 0;
                    case CodecProfileLevel.VP8Level_Version1:
                        return 1;
                    case CodecProfileLevel.VP8Level_Version2:
                        return 2;
                    case CodecProfileLevel.VP8Level_Version3:
                        return 3;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case CODEC_VP9:
                switch (level) {
                    case CodecProfileLevel.VP9Level1:
                        return 10;
                    case CodecProfileLevel.VP9Level11:
                        return 11;
                    case CodecProfileLevel.VP9Level2:
                        return 20;
                    case CodecProfileLevel.VP9Level21:
                        return 21;
                    case CodecProfileLevel.VP9Level3:
                        return 30;
                    case CodecProfileLevel.VP9Level31:
                        return 31;
                    case CodecProfileLevel.VP9Level4:
                        return 40;
                    case CodecProfileLevel.VP9Level41:
                        return 41;
                    case CodecProfileLevel.VP9Level5:
                        return 50;
                    case CodecProfileLevel.VP9Level51:
                        return 51;
                    case CodecProfileLevel.VP9Level52:
                        return 52;
                    case CodecProfileLevel.VP9Level6:
                        return 60;
                    case CodecProfileLevel.VP9Level61:
                        return 61;
                    case CodecProfileLevel.VP9Level62:
                        return 62;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case CODEC_HEVC:
                switch (level) {
                    case CodecProfileLevel.HEVCMainTierLevel1:
                        return 30;
                    case CodecProfileLevel.HEVCMainTierLevel2:
                        return 60;
                    case CodecProfileLevel.HEVCMainTierLevel21:
                        return 63;
                    case CodecProfileLevel.HEVCMainTierLevel3:
                        return 90;
                    case CodecProfileLevel.HEVCMainTierLevel31:
                        return 93;
                    case CodecProfileLevel.HEVCMainTierLevel4:
                        return 120;
                    case CodecProfileLevel.HEVCMainTierLevel41:
                        return 123;
                    case CodecProfileLevel.HEVCMainTierLevel5:
                        return 150;
                    case CodecProfileLevel.HEVCMainTierLevel51:
                        return 153;
                    case CodecProfileLevel.HEVCMainTierLevel52:
                        return 156;
                    case CodecProfileLevel.HEVCMainTierLevel6:
                        return 180;
                    case CodecProfileLevel.HEVCMainTierLevel61:
                        return 183;
                    case CodecProfileLevel.HEVCMainTierLevel62:
                        return 186;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            default:
                throw new UnsupportedCodecProfileException();
        }
    }
}
