**Note: This page is (somewhat) obsolete.** Chrome has a native 64-bit build now. Normal users should be running 64-bit Chrome on 64-bit systems. However, it's possible developers might be using a 64-bit system to build 32-bit Chrome, and will want to test the build on the same system. In that case, some of these tips may still be useful (though there might not be much more work done to address problems with this configuration).

Many 64-bit Linux distros allow you to run 32-bit apps but have many libraries misconfigured. The distros may be fixed at some point, but in the meantime we have workarounds.

## IME path wrong
Symptom: IME doesn't work.  `Gtk: /usr/lib/gtk-2.0/2.10.0/immodules/im-uim.so: wrong ELF class: ELFCLASS64`

Chromium bug: [9643](http://code.google.com/p/chromium/issues/detail?id=9643)

Affected systems:
| **Distro** | **upstream bug** |
|:-----------|:-----------------|
| Ubuntu Hardy, Jaunty | [190227](https://bugs.launchpad.net/ubuntu/+source/ia32-libs/+bug/190227) |

Workaround: If your xinput setting is to use SCIM, im-scim.so is searched for in lib32 directory, but ia32 package does not have im-scim.so in Ubuntu and perhaps other distributions. Ubuntu Hardy, however, has 32-bit im-xim.so. Therefore, invoking Chrome as following enables SCIM in Chrome.

> $ GTK\_IM\_MODULE=xim XMODIFIERS="@im=SCIM" chrome


## GTK filesystem module path wrong
Symptom: File picker doesn't work.  `Gtk: /usr/lib/gtk-2.0/2.10.0/filesystems/libgio.so: wrong ELF class: ELFCLASS64`

Chromium bug: [12151](http://code.google.com/p/chromium/issues/detail?id=12151)

Affected systems:
| **Distro** | **upstream bug** |
|:-----------|:-----------------|
| Ubuntu Hardy, Jaunty, Koala alpha 2 | [190227](https://bugs.launchpad.net/ubuntu/+source/ia32-libs/+bug/190227) |

Workaround: ??

## GIO module path wrong
Symptom: `/usr/lib/gio/modules/libgioremote-volume-monitor.so: wrong ELF class: ELFCLASS64`

Chromium bug: [12193](http://code.google.com/p/chromium/issues/detail?id=12193)

Affected systems:
| **Distro** | **upstream bug** |
|:-----------|:-----------------|
| Ubuntu Hardy, Jaunty, Koala alpha 2 | [190227](https://bugs.launchpad.net/ubuntu/+source/ia32-libs/+bug/190227) |

Workaround: ??

## Can't install on 64 bit Ubuntu 9.10 Live CD
Symptom: "Error: Dependency is not satisfiable: ia32-libs-gtk"

Chromium bug: n/a

Affected systems:
| **Distro** | **upstream bug** |
|:-----------|:-----------------|
| Ubuntu Koala alpha 2 |                  |

Workaround: Enable the Universe repository.  (It's enabled by default
when you actually install Ubuntu; only the live CD has it disabled.)

## gconv path wrong
Symptom: Paste doesn't work.  `Gdk: Error converting selection from STRING: Conversion from character set 'ISO-8859-1' to 'UTF-8' is not supported`

Chromium bug: [12312](http://code.google.com/p/chromium/issues/detail?id=12312)

Affected systems:
| **Distro** | **upstream bug** |
|:-----------|:-----------------|
| Arch       | ??               |

Workaround: Set `GCONV_PATH` to appropriate `/path/to/lib32/usr/lib/gconv` .