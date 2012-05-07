#!/bin/bash -e

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Builds Chromium, Google Chrome and *OS FFmpeg binaries.
#
# For Windows it assumes being run from a MinGW shell with Visual Studio
# environment (i.e., lib.exe and editbin.exe are in $PATH).
#
# Instructions for setting up a MinGW/MSYS shell can be found here:
# http://src.chromium.org/viewvc/chrome/trunk/deps/third_party/mingw/README.chromium

if [ "$3" = "" -o "$4" != "" ]; then
  echo "Usage:"
  echo "  $0 [TARGET_OS] [TARGET_ARCH] [path/to/third_party/ffmpeg]"
  echo
  echo "Valid combinations are linux [ia32|x64|arm|arm-neon]"
  echo "                       win   [ia32]"
  echo "                       mac   [ia32]"
  echo
  echo " linux ia32/x64 - script can be run on a normal Ubuntu box."
  echo " linux arm/arm-neon should be run inside of CrOS chroot."
  echo " mac and win have to be run on Mac and Windows 7 (under mingw)."
  echo
  echo " mac - ensure the Chromium (not Apple) version of clang is in the path,"
  echo " usually found under src/third_party/llvm-build/Release+Asserts/bin"
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
  exit
fi

TARGET_OS=$1
TARGET_ARCH=$2
FFMPEG_PATH=$3

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
LIBAVUTIL_VERSION_MAJOR=51

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
  Darwin*)
    HOST_OS=mac
    HOST_ARCH=ia32
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
  shift

  # Configure and check for exit status.
  echo "Configuring $CONFIG..."
  CMD="$FFMPEG_PATH/configure $*"
  echo $CMD
  eval $CMD

  if [ ! -f config.h ]; then
    echo "Configure failed!"
    exit 1
  fi

  # TODO(ihf): Remove munge_config_posix_memalign.sh when
  # DEPLOYMENT_TARGET >= 10.7. Background: If MACOSX_DEPLOYMENT_TARGET < 10.7
  # we have to disable posix_memalign by hand, as it was introduced somewhere
  # in OSX 10.6 (configure finds it) but we can't use it yet. Just check
  # common.gypi for 'mac_deployment_target%': '10.5'.
  if [ $TARGET_OS = "mac" ]; then
    # Required to get Mac ia32 builds compiling with -fno-omit-frame-pointer,
    # which is required for accurate stack traces.  See http://crbug.com/115170.
    if [ $TARGET_ARCH = "ia32" ]; then
      echo "Forcing HAVE_EBP_AVAILABLE to 0 in config.h and config.asm"
      $FFMPEG_PATH/chromium/scripts/munge_config_optimizations.sh config.h
      $FFMPEG_PATH/chromium/scripts/munge_config_optimizations.sh config.asm
    fi

    echo "Forcing POSIX_MEMALIGN to 0 in config.h and config.asm"
    $FFMPEG_PATH/chromium/scripts/munge_config_posix_memalign.sh config.h
    $FFMPEG_PATH/chromium/scripts/munge_config_posix_memalign.sh config.asm
  fi

  if [ "$HOST_OS" = "$TARGET_OS" ]; then
    # Build!
    LIBS="libavcodec/$(dso_name avcodec $LIBAVCODEC_VERSION_MAJOR)"
    LIBS="libavformat/$(dso_name avformat $LIBAVFORMAT_VERSION_MAJOR) $LIBS"
    LIBS="libavutil/$(dso_name avutil $LIBAVUTIL_VERSION_MAJOR) $LIBS"
    for lib in $LIBS; do
      echo "Building $lib for $CONFIG..."
      echo "make -j$JOBS $lib"
      make -j$JOBS $lib
      if [ -f $lib ]; then
        if [[ "$TARGET_ARCH" = "arm" ||
              "$TARGET_ARCH" = "arm-neon" ]]; then
          # Assume we are in chroot for CrOS.
          /usr/bin/armv7a-cros-linux-gnueabi-strip $lib
        elif [[ "$TARGET_OS" = "win" ]]; then
          # Windows doesn't care if global symbols get stripped, saving us a
          # cool 100KB.
          strip $lib

          # Windows binaries need /NXCOMPAT and /DYNAMICBASE.
          editbin -nxcompat -dynamicbase $lib
        else
          strip -x $lib
        fi

        cp $lib out
      else
        echo "Build failed!"
        exit
      fi
    done
  else
    echo "Skipping compile as host configuration differs from target."
    echo "Please compare the generated config.h with the previous version."
    echo "You may also patch the script to properly cross-compile."
    echo "host   OS  = $HOST_OS"
    echo "target OS  = $TARGET_OS"
    echo "host   ARCH= $HOST_ARCH"
    echo "target ARCH= $TARGET_ARCH"
  fi
  popd
}

# Common configuration.
add_flag_common --disable-doc
add_flag_common --disable-everything
add_flag_common --enable-fft
add_flag_common --enable-rdft
add_flag_common --disable-network
add_flag_common --disable-bzlib
add_flag_common --disable-zlib
add_flag_common --disable-swscale
add_flag_common --disable-amd3dnow
add_flag_common --disable-amd3dnowext
add_flag_common --enable-shared

# --optflags doesn't append multiple entries, so set all at once.
if [[ "$TARGET_OS" = "mac" && "$TARGET_ARCH" = "ia32" ]]; then
  add_flag_common --optflags="\"-fno-omit-frame-pointer -O2\""
else
  add_flag_common --optflags=-O2
fi

# Common codecs.
add_flag_common --enable-decoder=theora,vorbis,vp8
add_flag_common --enable-decoder=pcm_u8,pcm_s16le,pcm_f32le
add_flag_common --enable-demuxer=ogg,matroska,wav
add_flag_common --enable-parser=vp8

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
    # overriding everything live in chroot /build/*/etc/make.conf.
    # We try to follow these here closely. In particular we need to
    # set ffmpeg internal #defines to conform to make.conf.
    # TODO(ihf): For now it is not clear if thumb or arm settings would be
    # faster. I ran experiments in other contexts and performance seemed
    # to be close and compiler version dependent. In practice thumb builds are
    # much smaller than optimized arm builds, hence we go with the global
    # CrOS settings.
    add_flag_common --enable-armv6
    add_flag_common --enable-armv6t2
    add_flag_common --enable-armvfp
    add_flag_common --enable-thumb
    add_flag_common --disable-neon
    add_flag_common --extra-cflags=-march=armv7-a
    add_flag_common --extra-cflags=-mtune=cortex-a8
    add_flag_common --extra-cflags=-mfpu=vfpv3-d16
    add_flag_common --extra-cflags=-mfloat-abi=softfp
    # TODO(ihf): We are probably going to switch the whole tegra2 board soon to
    # add_flag_common --extra-cflags=-mfloat-abi=hard
  elif [ "$TARGET_ARCH" = "arm-neon" ]; then
    # This if-statement is for chroot arm-generic.
    add_flag_common --enable-cross-compile
    add_flag_common --cross-prefix=/usr/bin/armv7a-cros-linux-gnueabi-
    add_flag_common --target-os=linux
    add_flag_common --arch=arm
    add_flag_common --enable-armv6
    add_flag_common --enable-armv6t2
    add_flag_common --enable-armvfp
    add_flag_common --enable-thumb
    add_flag_common --enable-neon
    add_flag_common --extra-cflags=-march=armv7-a
    add_flag_common --extra-cflags=-mtune=cortex-a8
    add_flag_common --extra-cflags=-mfpu=neon
    add_flag_common --extra-cflags=-mfloat-abi=softfp
  else
    echo "Error: Unknown TARGET_ARCH=$TARGET_ARCH for TARGET_OS=$TARGET_OS!"
    exit
  fi
fi

# Should be run on Windows.
if [ "$TARGET_OS" = "win" ]; then
  if [ "$HOST_OS" = "win" ]; then
    if [ "$TARGET_ARCH" = "ia32" ]; then
      add_flag_common --enable-filter=buffer
      add_flag_common --enable-memalign-hack
      add_flag_common --cc=mingw32-gcc
      add_flag_common --extra-cflags=-mtune=atom
      add_flag_common --extra-cflags=-U__STRICT_ANSI__
      add_flag_common --extra-cflags=-I/usr/local/include
      add_flag_common --extra-ldflags=-L/usr/local/lib
      add_flag_common --extra-ldflags=-Wl,--enable-auto-import
      add_flag_common --extra-ldflags=-Wl,--no-seh
    else
      echo "Error: Unknown TARGET_ARCH=$TARGET_ARCH for TARGET_OS=$TARGET_OS!"
      exit
    fi
  else
    echo "Script should be run on Windows host. If this is not possible try a "
    echo "merge of config files with new linux ia32 config.h by hand."
    exit
  fi
else
  add_flag_common --enable-pic
fi

# Should be run on Mac.
if [ "$TARGET_OS" = "mac" ]; then
  if [ "$HOST_OS" = "mac" ]; then
    if [ "$TARGET_ARCH" = "ia32" ]; then
      add_flag_common --arch=i686
      add_flag_common --enable-yasm
      add_flag_common --extra-cflags=-m32
      add_flag_common --extra-ldflags=-m32
      add_flag_common --cc=clang
      add_flag_common --cxx=clang++
    else
      echo "Error: Unknown TARGET_ARCH=$TARGET_ARCH for TARGET_OS=$TARGET_OS!"
      exit
    fi
  else
    echo "Script should be run on a Mac host. If this is not possible try a "
    echo "merge of config files with new linux ia32 config.h by hand."
    exit
  fi
  # Configure seems to enable VDA despite --disable-everything if you have
  # XCode installed, so force disable it.
  add_flag_common --disable-vda
fi

# Chromium & ChromiumOS specific configuration.
# (nothing at the moment)

# Google Chrome & ChromeOS specific configuration.
add_flag_chrome --enable-decoder=aac,h264,mp3
add_flag_chrome --enable-demuxer=mp3,mov
add_flag_chrome --enable-parser=mpegaudio
add_flag_chrome --enable-bsf=h264_mp4toannexb

# ChromiumOS specific configuration.
# Warning: do *NOT* add avi, aac, h264, mp3, mp4, amr*
# Flac support.
add_flag_chromiumos --enable-demuxer=flac
add_flag_chromiumos --enable-decoder=flac

# Google ChromeOS specific configuration.
# We want to make sure to play everything Android generates and plays.
# http://developer.android.com/guide/appendix/media-formats.html
# Enable playing avi files.
add_flag_chromeos --enable-decoder=mpeg4
add_flag_chromeos --enable-demuxer=avi
add_flag_chromeos --enable-bsf=mpeg4video_es
# Enable playing Android 3gp files.
add_flag_chromeos --enable-demuxer=amr
add_flag_chromeos --enable-decoder=amrnb,amrwb
# Flac support.
add_flag_chromeos --enable-demuxer=flac
add_flag_chromeos --enable-decoder=flac
# Wav files for playing phone messages.
# Maybe later: gsm_ms,adpcm_ima_wav
add_flag_chromeos --enable-decoder=pcm_mulaw

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
