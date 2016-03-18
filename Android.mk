LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE:= libwebm
LOCAL_SRC_FILES:= common/file_util.cc \
                  common/hdr_util.cc \
                  mkvparser/mkvparser.cpp \
                  mkvparser/mkvreader.cpp \
                  mkvmuxer/mkvmuxer.cpp \
                  mkvmuxer/mkvmuxerutil.cpp \
                  mkvmuxer/mkvwriter.cpp
include $(BUILD_STATIC_LIBRARY)
