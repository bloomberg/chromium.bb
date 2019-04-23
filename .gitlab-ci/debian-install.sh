#!/bin/bash

set -o xtrace

echo 'deb http://deb.debian.org/debian buster-backports main' >> /etc/apt/sources.list
apt-get update
apt-get -y --no-install-recommends install build-essential automake autoconf libtool pkg-config libexpat1-dev libffi-dev libxml2-dev libpixman-1-dev libpng-dev libjpeg-dev libcolord-dev mesa-common-dev libglu1-mesa-dev libegl1-mesa-dev libgles2-mesa-dev libwayland-dev libxcb1-dev libxcb-composite0-dev libxcb-xfixes0-dev libxcb-xkb-dev libx11-xcb-dev libx11-dev libudev-dev libgbm-dev libxkbcommon-dev libcairo2-dev libpango1.0-dev libgdk-pixbuf2.0-dev libxcursor-dev libmtdev-dev libpam0g-dev libvpx-dev libsystemd-dev libevdev-dev libinput-dev libwebp-dev libjpeg-dev libva-dev liblcms2-dev git libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev freerdp2-dev curl python3-pip python3-setuptools doxygen ninja-build libdbus-1-dev libpipewire-0.2-dev

pip3 install --user git+https://github.com/mesonbuild/meson.git@0.49
# for documentation
pip3 install sphinx==2.1.0 --user
pip3 install breathe==4.13.0.post0 --user
pip3 install sphinx_rtd_theme==0.4.3 --user

mkdir -p /tmp/.X11-unix
chmod 777 /tmp/.X11-unix
