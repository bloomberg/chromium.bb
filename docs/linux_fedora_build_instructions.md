# Building Chromium on Fedora

Generally speaking, follow the [Linux Build Instructions](linux_build_instructions.md).
However, do the following instead to install the build dependenies:

Generally, follow the main [Linux Build instructions], but instead of
running `build/install-build-deps.sh`, run:

    su -c 'yum install git python bzip2 tar pkgconfig atk-devel alsa-lib-devel \
    bison binutils brlapi-devel bluez-libs-devel bzip2-devel cairo-devel \
    cups-devel dbus-devel dbus-glib-devel expat-devel fontconfig-devel \
    freetype-devel gcc-c++ GConf2-devel glib2-devel glibc.i686 gperf \
    glib2-devel gtk2-devel gtk3-devel java-1.*.0-openjdk-devel libatomic \
    libcap-devel libffi-devel libgcc.i686 libgnome-keyring-devel libjpeg-devel \
    libstdc++.i686 libX11-devel libXScrnSaver-devel libXtst-devel \
    libxkbcommon-x11-devel ncurses-compat-libs nspr-devel nss-devel pam-devel \
    pango-devel pciutils-devel pulseaudio-libs-devel zlib.i686 httpd mod_ssl \
    php php-cli python-psutil wdiff'

The msttcorefonts packages can be obtained by following the instructions
present [here](http://www.fedorafaq.org/#installfonts). For the optional
packages:

*   php-cgi is provided by the php-cli package
*   sun-java6-fonts doesn't exist in Fedora repositories, needs investigating
