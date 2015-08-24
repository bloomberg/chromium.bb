# Introduction

The msttcorefonts are needed to build Chrome but are not available for Mandriva. Building your own is not hard though and only takes about 2 minutes to set up and complete


# Details

```
urpmi rpm-build cabextract
```

download this script, make it executable and run it http://wiki.mandriva.com/en/uploads/3/3a/Rpmsetup.sh it will create a directory ~/rpm and some hidden files in your home directory

open the file ~/.rpmmacros and comment out the following lines by putting a # in front of them, eg like this (because most likely you won't have a gpg key set up and creating the package will fail if you leave these lines):

#%_signature             gpg_

#%_gpg\_name              Mandrivalinux_

#%_gpg\_path              ~/.gnupg_

download the following file http://corefonts.sourceforge.net/msttcorefonts-2.0-1.spec and save it to ~/rpm/SPECS

```
cd ~/rpm/SPECS

rpmbuild -bb msttcorefonts-2.0-1.spec
```

the rpm will be build and be put in ~/rpm/RPMS/noarch ready to install