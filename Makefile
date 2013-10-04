# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CHROME=google-chrome

# Include all resources of the Camera App to be copied to the target package,
# but without the manifest files.
SRC_RESOURCES= \
	src/_locales/en/messages.json \
	src/css/main.css \
	src/images/2x/camera_button_picture.png \
	src/images/2x/camera_button_video.png \
	src/images/2x/topbar_button_close.png \
	src/images/2x/topbar_button_gallery.png \
	src/images/2x/topbar_button_maximize.png \
	src/images/camera_app_icons_128.png \
	src/images/camera_app_icons_256.png \
	src/images/camera_app_icons_32.png \
	src/images/camera_app_icons_48.png \
	src/images/camera_app_icons_64.png \
	src/images/camera_app_icons_96.png \
	src/images/camera_app_icons_favicon_16.png \
	src/images/camera_app_icons_favicon_32.png \
	src/images/camera_button_picture.png \
	src/images/camera_button_video.png \
	src/images/no_camera.svg \
	src/images/topbar_button_close.png \
	src/images/topbar_button_gallery.png \
	src/images/topbar_button_maximize.png \
	src/js/background.js \
	src/js/effect.js \
	src/js/effects/andy.js \
	src/js/effects/cinema.js \
	src/js/effects/colorize.js \
	src/js/effects/funky.js \
	src/js/effects/grayscale.js \
	src/js/effects/newspaper.js \
	src/js/effects/normal.js \
	src/js/effects/pinch.js \
	src/js/effects/sepia.js \
	src/js/effects/swirl.js \
	src/js/effects/tilt-shift.js \
	src/js/effects/vintage.js \
	src/js/main.js \
	src/js/processor.js \
	src/js/test.js \
	src/js/test_cases.js \
	src/js/tracker.js \
	src/js/util.js \
	src/js/view.js \
	src/js/views/camera.js \
	src/js/views/gallery.js \
	src/sounds/shutter.wav \
	src/views/main.html \

# Path for the Camera resources. Relative, with a trailing slash.
SRC_PATH=src/

# Manifest file for the camera.crx package.
SRC_MANIFEST=src/manifest.json

# Manifest file for the tests.crx package.
SRC_TESTS_MANIFEST=src/manifest-tests.json

# Resources of the third party glfx library. They will be built, and the target
# glfx.js with LICENSE will be copied to the target package.
GLFX_RESOURCES= \
	third_party/glfx/LICENSE \
	third_party/glfx/build.py \
	third_party/glfx/src/OES_texture_float_linear-polyfill.js \
	third_party/glfx/src/core/canvas.js \
	third_party/glfx/src/core/matrix.js \
	third_party/glfx/src/core/shader.js \
	third_party/glfx/src/core/spline.js \
	third_party/glfx/src/core/texture.js \
	third_party/glfx/src/filters/adjust/brightnesscontrast.js \
	third_party/glfx/src/filters/adjust/curves.js \
	third_party/glfx/src/filters/adjust/denoise.js \
	third_party/glfx/src/filters/adjust/huesaturation.js \
	third_party/glfx/src/filters/adjust/noise.js \
	third_party/glfx/src/filters/adjust/sepia.js \
	third_party/glfx/src/filters/adjust/unsharpmask.js \
	third_party/glfx/src/filters/adjust/vibrance.js \
	third_party/glfx/src/filters/adjust/vignette.js \
	third_party/glfx/src/filters/blur/lensblur.js \
	third_party/glfx/src/filters/blur/tiltshift.js \
	third_party/glfx/src/filters/blur/triangleblur.js \
	third_party/glfx/src/filters/blur/zoomblur.js \
	third_party/glfx/src/filters/common.js \
	third_party/glfx/src/filters/fun/colorhalftone.js \
	third_party/glfx/src/filters/fun/dotscreen.js \
	third_party/glfx/src/filters/fun/edgework.js \
	third_party/glfx/src/filters/fun/hexagonalpixelate.js \
	third_party/glfx/src/filters/fun/ink.js \
	third_party/glfx/src/filters/warp/bulgepinch.js \
	third_party/glfx/src/filters/warp/matrixwarp.js \
	third_party/glfx/src/filters/warp/perspective.js \
	third_party/glfx/src/filters/warp/swirl.js \
	third_party/glfx/www/build.py \

# Resouces of the third party ccv library. All of these files will be copied to
# the target package.
CCV_RESOURCES= \
	third_party/ccv/js/face.js \
	third_party/ccv/js/ccv.js \
	third_party/ccv/COPYING \

# Builds camera.crx and tests.crx
all: build/camera.crx build/tests.crx

# Builds the glfx third_party library.
build/third_party/glfx: $(GLFX_RESOURCES)
	mkdir -p build
	cp --parents $(GLFX_RESOURCES) build
	cd build/third_party/glfx && ./build.py

# Copies the built glfx library to the camera.crx build directory.
build/camera/js/third_party/glfx: $(GLFX_RESOURCES) build/third_party/glfx
	mkdir -p build/camera/js/third_party/glfx
	cp build/third_party/glfx/glfx.js build/camera/js/third_party/glfx
	cp build/third_party/glfx/LICENSE build/camera/js/third_party/glfx

# Copies the built library to the tests.crx build directory.
build/tests/js/third_party/glfx: $(GLFX_RESOURCES) build/third_party/glfx
	mkdir -p build/tests/js/third_party/glfx
	cp build/third_party/glfx/glfx.js build/tests/js/third_party/glfx
	cp build/third_party/glfx/LICENSE build/tests/js/third_party/glfx

# Copies the ccv library to the camera.crx build directory.
build/camera/js/third_party/ccv: $(CCV_RESOURCES)
	mkdir -p build/camera/js
	cp --parents $(CCV_RESOURCES) build/camera/js

# Copies the ccv library to the tests.crx build directory.
build/tests/js/third_party/ccv: $(CCV_RESOURCES)
	mkdir -p build/tests/js
	cp --parents $(CCV_RESOURCES) build/tests/js

# Builds the camera.crx package.
build/camera.crx: $(SRC_RESOURCES) $(SRC_MANIFEST) \
	  build/camera/js/third_party/glfx \
	  build/camera/js/third_party/ccv
	mkdir -p build/camera
	cd $(SRC_PATH); cp --parents $(patsubst $(SRC_PATH)%, %, \
	  $(SRC_RESOURCES)) ../build/camera
	cp $(SRC_MANIFEST) build/camera/manifest.json
	$(CHROME) --pack-extension=build/camera \
	  --pack-extension-key=dev-keys/camera.pem

# Alias for build/camera.crx.
camera: build/camera.crx

# Builds the tests.crx package.
build/tests.crx: $(SRC_RESOURCES) $(SRC_TESTS_MANIFEST) \
	  build/tests/js/third_party/glfx \
	  build/tests/js/third_party/ccv
	mkdir -p build/tests
	cd $(SRC_PATH); cp --parents $(patsubst $(SRC_PATH)%, %, \
	  $(SRC_RESOURCES)) ../build/tests
	cp $(SRC_TESTS_MANIFEST) build/tests/manifest.json
	$(CHROME) --pack-extension=build/tests \
	  --pack-extension-key=dev-keys/camera.pem

# Alias for build/tests.crx.
tests: build/tests.crx

# Cleans the workspace from build files files.
clean:
	rm -rf build

# Builds the tests.crx package and performs tests on it.
run-tests: build/tests.crx
	tests/run_all_tests.py
