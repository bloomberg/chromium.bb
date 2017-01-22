LOCAL_CFLAGS += \
	-DHAVE_LIBDRM_ATOMIC_PRIMITIVES=1

# Quiet down the build system and remove any .h files from the sources
LOCAL_SRC_FILES := $(patsubst %.h, , $(LOCAL_SRC_FILES))
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)
