LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE:= libmkvparser
LOCAL_SRC_FILES:= mkvparser.cpp \
                  mkvreader.cpp
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE:= mkvparser
LOCAL_SRC_FILES:= sample.cpp
LOCAL_STATIC_LIBRARIES:= libmkvparser
include $(BUILD_EXECUTABLE)
