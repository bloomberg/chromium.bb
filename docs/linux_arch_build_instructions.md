# Building Chromium on Arch Linux

Generally speaking, follow the [Linux Build Instructions](linux_build_instructions.md).
However, do the following instead to install the build dependencies:

## Install the build dependencies:

Most of these packages are probably already installed since they're often used,
and the parameter --needed ensures that packages up to date are not reinstalled.

    sudo pacman -S --needed python perl gcc gcc-libs bison flex gperf pkgconfig \
    nss alsa-lib gconf glib2 gtk2 nspr ttf-ms-fonts freetype2 cairo dbus \
    libgnome-keyring

For the optional packages on Arch Linux:

*   php-cgi is provided with pacman
*   wdiff is not in the main repository but dwdiff is. You can get wdiff in
    AUR/yaourt
*   sun-java6-fonts do not seem to be in main repository or AUR.
