#!/bin/bash -e

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Builds Chromium, Google Chrome and *OS FFmpeg binaries.
#
# For Windows it the script must be run from either a x64 or ia32 Visual Studio
# environment (i.e., cl.exe, lib.exe and editbin.exe are in $PATH).  Using the
# x64 environment will build the x64 version and vice versa.
#
# For MIPS it assumes that cross-toolchain bin directory is in $PATH.
#
# Instructions for setting up a MinGW/MSYS shell can be found here:
# http://src.chromium.org/viewvc/chrome/trunk/deps/third_party/mingw/README.chromium

if [ "$3" = "" ]; then
  echo "Usage:"
  echo "  $0 [TARGET_OS] [TARGET_ARCH] [path/to/ffmpeg] [config-only]"
  echo
  echo "Valid combinations are linux [ia32|x64|mipsel|arm|arm-neon]"
  echo "                       win   [ia32|x64]"
  echo "                       mac   [ia32|x64]"
  echo
  echo " linux ia32/x64 - script can be run on a normal Ubuntu box."
  echo " linux mipsel - script can be run on a normal Ubuntu box with MIPS"
  echo " cross-toolchain in \$PATH."
  echo " linux arm/arm-neon should be run inside of CrOS chroot."
  echo " mac and win have to be run on Mac and Windows 7 (under mingw)."
  echo
  echo " mac - ensure the Chromium (not Apple) version of clang is in the path,"
  echo " usually found under src/third_party/llvm-build/Release+Asserts/bin"
  echo
  echo "Specifying 'config-only' will skip the build step.  Useful when a"
  echo "given platform is not necessary for generate_gyp.py."
  echo
  echo "The path should be absolute and point at Chromium's copy of FFmpeg."
  echo "This corresponds to:"
  echo "  chrome/trunk/deps/third_party/ffmpeg"
  echo
  echo "Resulting binaries will be placed in:"
  echo "  build.TARGET_ARCH.TARGET_OS/Chromium/out/"
  echo "  build.TARGET_ARCH.TARGET_OS/Chrome/out/"
  echo "  build.TARGET_ARCH.TARGET_OS/ChromiumOS/out/"
  echo "  build.TARGET_ARCH.TARGET_OS/ChromeOS/out/"
  exit 1
fi

TARGET_OS=$1
TARGET_ARCH=$2
FFMPEG_PATH=$3
CONFIG_ONLY=$4

# Check TARGET_OS (TARGET_ARCH is checked during configuration).
if [[ "$TARGET_OS" != "linux" &&
      "$TARGET_OS" != "mac" &&
      "$TARGET_OS" != "win" ]]; then
  echo "Valid target OSes are: linux, mac, win"
  exit 1
fi

# Check FFMPEG_PATH to contain this script.
if [ ! -f "$FFMPEG_PATH/chromium/scripts/$(basename $0)" ]; then
  echo "$FFMPEG_PATH doesn't appear to contain FFmpeg"
  exit 1
fi

# If configure & make works but this script doesn't, make sure to grep for
# these.
LIBAVCODEC_VERSION_MAJOR=54
LIBAVFORMAT_VERSION_MAJOR=54
LIBAVUTIL_VERSION_MAJOR=52

case $(uname -sm) in
  Linux\ i386)
    HOST_OS=linux
    HOST_ARCH=ia32
    JOBS=$(grep processor /proc/cpuinfo | wc -l)
    ;;
  Linux\ x86_64)
    HOST_OS=linux
    HOST_ARCH=x64
    JOBS=$(grep processor /proc/cpuinfo | wc -l)
    ;;
  Darwin\ i386)
    HOST_OS=mac
    HOST_ARCH=ia32
    JOBS=$(sysctl -n hw.ncpu)
    ;;
  Darwin\ x86_64)
    HOST_OS=mac
    HOST_ARCH=x64
    JOBS=$(sysctl -n hw.ncpu)
    ;;
  MINGW*)
    HOST_OS=win
    HOST_ARCH=ia32
    JOBS=$NUMBER_OF_PROCESSORS
    ;;
  *)
    echo "Unrecognized HOST_OS: $(uname)"
    echo "Patches welcome!"
    exit 1
    ;;
esac

# Print out system information.
echo "System information:"
echo "HOST_OS     = $HOST_OS"
echo "TARGET_OS   = $TARGET_OS"
echo "HOST_ARCH   = $HOST_ARCH"
echo "TARGET_ARCH = $TARGET_ARCH"
echo "JOBS        = $JOBS"
echo "LD          = $(ld --version | head -n1)"
echo

# As of this writing gold 2.20.1-system.20100303 is unable to link FFmpeg.
if ld --version | grep -q gold; then
  echo "gold is unable to link FFmpeg"
  echo
  echo "Switch /usr/bin/ld to the regular binutils ld and try again"
  exit 1
fi

# We want to use a sufficiently recent version of yasm on Windows.
if [ "$TARGET_OS" == "win" ]; then
  if !(which yasm 2>&1 > /dev/null); then
    echo "Could not find yasm in \$PATH"
    exit 1
  fi

  if (yasm --version | head -1 | grep -q "^yasm 0\."); then
    echo "Must have yasm 1.0 or higher installed"
    exit 1
  fi
fi

# Returns the Dynamic Shared Object name given the module name and version.
function dso_name { dso_name_${TARGET_OS} $1 $2; }
function dso_name_win { echo "${1}-${2}.dll"; }
function dso_name_linux { echo "lib${1}.so.${2}"; }
function dso_name_mac { echo "lib${1}.${2}.dylib"; }

# Appends configure flags.
FLAGS_COMMON=
FLAGS_CHROMIUM=
FLAGS_CHROME=
FLAGS_CHROMIUMOS=
FLAGS_CHROMEOS=

# Flags that are used in all of Chrome/Chromium + OS.
function add_flag_common {
  FLAGS_COMMON="$FLAGS_COMMON $*"
}
# Flags that are used in Chromium and ChromiumOS.
function add_flag_chromium {
  FLAGS_CHROMIUM="$FLAGS_CHROMIUM $*"
}
# Flags that are used in Chrome and ChromeOS.
function add_flag_chrome {
  FLAGS_CHROME="$FLAGS_CHROME $*"
}
# Flags that are used only in ChromiumOS (but not Chromium).
function add_flag_chromiumos {
  FLAGS_CHROMIUMOS="$FLAGS_CHROMIUMOS $*"
}
# Flags that are used only in ChromeOS (but not Chrome).
function add_flag_chromeos {
  FLAGS_CHROMEOS="$FLAGS_CHROMEOS $*"
}

# Builds using $1 as the output directory and all following arguments as
# configure script arguments.
function build {
  CONFIG=$1
  CONFIG_DIR="build.$TARGET_ARCH.$TARGET_OS/$CONFIG"
  shift

  # Create our working directory.
  echo "Creating build directory..."
  rm -rf $CONFIG_DIR
  mkdir -p $CONFIG_DIR
  pushd $CONFIG_DIR
  mkdir out

  # Configure and check for exit status.
  echo "Configuring $CONFIG..."
  CMD="$FFMPEG_PATH/configure $*"
  echo $CMD
  eval $CMD

  if [ ! -f config.h ]; then
    echo "Configure failed!"
    exit 1
  fi

  if [ $TARGET_OS = "mac" ]; then
    # Required to get Mac ia32 builds compiling with -fno-omit-frame-pointer,
    # which is required for accurate stack traces.  See http://crbug.com/115170.
    if [ $TARGET_ARCH = "ia32" ]; then
      echo "Forcing HAVE_EBP_AVAILABLE to 0 in config.h and config.asm"
      $FFMPEG_PATH/chromium/scripts/munge_config_optimizations.sh config.h
      $FFMPEG_PATH/chromium/scripts/munge_config_optimizations.sh config.asm
    fi
  fi

  # Disable inclusion of external iconv library
  echo "Forcing CONFIG_ICONV to 0 in config.h"
  $FFMPEG_PATH/chromium/scripts/munge_config_iconv.sh config.h
  if [[ "$TARGET_ARCH" = "ia32" || "$TARGET_ARCH" = "x64" ]]; then
    echo "Forcing CONFIG_ICONV to 0 in config.asm"
    $FFMPEG_PATH/chromium/scripts/munge_config_iconv.sh config.asm
  fi

  if [[ "$HOST_OS" = "$TARGET_OS" && "$CONFIG_ONLY" = "" ]]; then
    # Build!
    LIBS="libavcodec/$(dso_name avcodec $LIBAVCODEC_VERSION_MAJOR)"
    LIBS="libavformat/$(dso_name avformat $LIBAVFORMAT_VERSION_MAJOR) $LIBS"
    LIBS="libavutil/$(dso_name avutil $LIBAVUTIL_VERSION_MAJOR) $LIBS"
    for lib in $LIBS; do
      echo "Building $lib for $CONFIG..."
      echo "make -j$JOBS $lib"
      make -j$JOBS $lib
      if [ -f $lib ]; then
        cp $lib out
      else
        echo "Build failed!"
        exit 1
      fi
    done
  elif [ ! "$CONFIG_ONLY" = "" ]; then
    echo "Skipping build step as requested."
  else
    echo "Skipping compile as host configuration differs from target."
    echo "Please compare the generated config.h with the previous version."
    echo "You may also patch the script to properly cross-compile."
    echo "host   OS  = $HOST_OS"
    echo "target OS  = $TARGET_OS"
    echo "host   ARCH= $HOST_ARCH"
    echo "target ARCH= $TARGET_ARCH"
  fi

  if [ "$TARGET_ARCH" = "arm" -o "$TARGET_ARCH" = "arm-neon" ]; then
    sed -i 's/^\(#define HAVE_VFP_ARGS [01]\)$/\/* \1 -- Disabled to allow softfp\/hardfp selection at gyp time *\//' config.h
  fi

  popd
}

# Common configuration.  Note: --disable-everything does not in fact disable
# everything, just non-library components such as decoders and demuxers.
add_flag_common --disable-everything
add_flag_common --disable-avdevice
add_flag_common --disable-avfilter
add_flag_common --disable-bzlib
add_flag_common --disable-doc
add_flag_common --disable-ffprobe
add_flag_common --disable-lzo
add_flag_common --disable-network
add_flag_common --disable-postproc
add_flag_common --disable-swresample
add_flag_common --disable-swscale
add_flag_common --disable-zlib
add_flag_common --enable-fft
add_flag_common --enable-rdft
add_flag_common --enable-shared

# Disable hardware decoding options which will sometimes turn on via autodetect.
add_flag_common --disable-dxva2
add_flag_common --disable-vaapi
add_flag_common --disable-vda
add_flag_common --disable-vdpau

# --optflags doesn't append multiple entries, so set all at once.
if [[ "$TARGET_OS" = "mac" && "$TARGET_ARCH" = "ia32" ]]; then
  add_flag_common --optflags="\"-fno-omit-frame-pointer -O2\""
else
  add_flag_common --optflags=-O2
fi

# Common codecs.
add_flag_common --enable-decoder=theora,vorbis,vp8
add_flag_common --enable-decoder=pcm_u8,pcm_s16le,pcm_s24le,pcm_f32le
add_flag_common --enable-decoder=pcm_s16be,pcm_s24be
add_flag_common --enable-demuxer=ogg,matroska,wav
add_flag_common --enable-parser=vp3,vorbis,vp8

# Linux only.
if [ "$TARGET_OS" = "linux" ]; then
  if [ "$TARGET_ARCH" = "x64" ]; then
    # Nothing to add for now.
    add_flag_common ""
  elif [ "$TARGET_ARCH" = "ia32" ]; then
    add_flag_common --arch=i686
    add_flag_common --enable-yasm
    add_flag_common --extra-cflags=-m32
    add_flag_common --extra-ldflags=-m32
  elif [ "$TARGET_ARCH" = "arm" ]; then
    # This if-statement essentially is for chroot tegra2.
    add_flag_common --enable-cross-compile

    # Location is for CrOS chroot. If you want to use this, enter chroot
    # and copy ffmpeg to a location that is reachable.
    add_flag_common --cross-prefix=/usr/bin/armv7a-cros-linux-gnueabi-
    add_flag_common --target-os=linux
    add_flag_common --arch=arm

    # TODO(ihf): ARM compile flags are tricky. The final options
    # overriding everything live in chroot /build/*/etc/make.conf
    # (some of them coming from src/overlays/overlay-<BOARD>/make.conf).
    # We try to follow these here closely. In particular we need to
    # set ffmpeg internal #defines to conform to make.conf.
    # TODO(ihf): For now it is not clear if thumb or arm settings would be
    # faster. I ran experiments in other contexts and performance seemed
    # to be close and compiler version dependent. In practice thumb builds are
    # much smaller than optimized arm builds, hence we go with the global
    # CrOS settings.
    add_flag_common --enable-armv6
    add_flag_common --enable-armv6t2
    add_flag_common --enable-vfp
    add_flag_common --enable-thumb
    add_flag_common --disable-neon
    add_flag_common --extra-cflags=-march=armv7-a
    add_flag_common --extra-cflags=-mtune=cortex-a8
    add_flag_common --extra-cflags=-mfpu=vfpv3-d16
    # NOTE: softfp/hardfp selected at gyp time.
    add_flag_common --extra-cflags=-mfloat-abi=hard
  elif [ "$TARGET_ARCH" = "arm-neon" ]; then
    # This if-statement is for chroot arm-generic.
    add_flag_common --enable-cross-compile
    add_flag_common --cross-prefix=/usr/bin/armv7a-cros-linux-gnueabi-
    add_flag_common --target-os=linux
    add_flag_common --arch=arm
    add_flag_common --enable-armv6
    add_flag_common --enable-armv6t2
    add_flag_common --enable-vfp
    add_flag_common --enable-thumb
    add_flag_common --enable-neon
    add_flag_common --extra-cflags=-march=armv7-a
    add_flag_common --extra-cflags=-mtune=cortex-a8
    add_flag_common --extra-cflags=-mfpu=neon
    # NOTE: softfp/hardfp selected at gyp time.
    add_flag_common --extra-cflags=-mfloat-abi=hard
  elif [ "$TARGET_ARCH" = "mipsel" ]; then
    add_flag_common --enable-cross-compile
    add_flag_common --cross-prefix=mips-linux-gnu-
    add_flag_common --target-os=linux
    add_flag_common --arch=mips
    add_flag_common --extra-cflags=-mips32
    add_flag_common --extra-cflags=-EL
    add_flag_common --extra-ldflags=-mips32
    add_flag_common --extra-ldflags=-EL
    add_flag_common --disable-mipsfpu
    add_flag_common --disable-mipsdspr1
    add_flag_common --disable-mipsdspr2
  else
    echo "Error: Unknown TARGET_ARCH=$TARGET_ARCH for TARGET_OS=$TARGET_OS!"
    exit 1
  fi
fi

# Should be run on Windows.
if [ "$TARGET_OS" = "win" ]; then
  if [ "$HOST_OS" = "win" ]; then
    add_flag_common --toolchain=msvc
    add_flag_common --enable-yasm
    add_flag_common --extra-cflags=-I$FFMPEG_PATH/chromium/include/win
  else
    echo "Script should be run on Windows host. If this is not possible try a "
    echo "merge of config files with new linux ia32 config.h by hand."
    exit 1
  fi
else
  add_flag_common --enable-pic
fi

# Should be run on Mac.
if [ "$TARGET_OS" = "mac" ]; then
  if [ "$HOST_OS" = "mac" ]; then
    add_flag_common --enable-yasm
    add_flag_common --cc=clang
    add_flag_common --cxx=clang++
    if [ "$TARGET_ARCH" = "ia32" ]; then
      add_flag_common --arch=i686
      add_flag_common --extra-cflags=-m32
      add_flag_common --extra-ldflags=-m32
    elif [ "$TARGET_ARCH" = "x64" ]; then
      add_flag_common --arch=x86_64
      add_flag_common --extra-cflags=-m64
      add_flag_common --extra-ldflags=-m64
    else
      echo "Error: Unknown TARGET_ARCH=$TARGET_ARCH for TARGET_OS=$TARGET_OS!"
      exit 1
    fi
  else
    echo "Script should be run on a Mac host. If this is not possible try a "
    echo "merge of config files with new linux ia32 config.h by hand."
    exit 1
  fi
fi

# Chromium & ChromiumOS specific configuration.
# (nothing at the moment)

# Google Chrome & ChromeOS specific configuration.
add_flag_chrome --enable-decoder=aac,h264,mp3
add_flag_chrome --enable-demuxer=mp3,mov
add_flag_chrome --enable-parser=aac,h264,mpegaudio

# ChromiumOS specific configuration.
# Warning: do *NOT* add avi, aac, h264, mp3, mp4, amr*
# Flac support.
add_flag_chromiumos --enable-demuxer=flac
add_flag_chromiumos --enable-decoder=flac
add_flag_chromiumos --enable-parser=flac

# Google ChromeOS specific configuration.
# We want to make sure to play everything Android generates and plays.
# http://developer.android.com/guide/appendix/media-formats.html
# Enable playing avi files.
add_flag_chromeos --enable-decoder=mpeg4
add_flag_chromeos --enable-parser=h263,mpeg4video
add_flag_chromeos --enable-demuxer=avi
add_flag_chromeos --enable-bsf=mpeg4video_es
# Enable playing Android 3gp files.
add_flag_chromeos --enable-demuxer=amr
add_flag_chromeos --enable-decoder=amrnb,amrwb
# Flac support.
add_flag_chromeos --enable-demuxer=flac
add_flag_chromeos --enable-decoder=flac
add_flag_chromeos --enable-parser=flac
# Wav files for playing phone messages.
add_flag_chromeos --enable-decoder=pcm_mulaw
add_flag_chromeos --enable-decoder=gsm_ms
add_flag_chromeos --enable-demuxer=gsm
add_flag_chromeos --enable-parser=gsm

echo "Chromium configure/build:"
build Chromium $FLAGS_COMMON $FLAGS_CHROMIUM
echo "Chrome configure/build:"
build Chrome $FLAGS_COMMON $FLAGS_CHROME

if [ "$TARGET_OS" = "linux" ]; then
  echo "ChromiumOS configure/build:"
  build ChromiumOS $FLAGS_COMMON $FLAGS_CHROMIUM $FLAGS_CHROMIUMOS
  echo "ChromeOS configure/build:"
  build ChromeOS $FLAGS_COMMON $FLAGS_CHROME $FLAGS_CHROMEOS
fi

echo "Done. If desired you may copy config.h/config.asm into the" \
     "source/config tree using copy_config.sh."
