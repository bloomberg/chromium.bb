FROM ubuntu:trusty

RUN apt-get update && \
  apt-get install -yq libasound2 libatk1.0-0 libavahi-client3 libavahi-common3 \
  libcairo2 libcups2 libdatrie1 libdbus-glib-1-2 libffi6 libfontconfig1 \
  libgconf-2-4 libglib2.0-0 libgnutls26 libgraphite2-3 libgssapi-krb5-2 \
  libk5crypto3 libkrb5-3 libkrb5support0 libnspr4 libnss3 libotf0 libp11-kit0 \
  libpango1.0-0  libpixman-1-0 libstdc++6 libtasn1-6 libthai0 libx11-6 libxau6 \
  libxcb1 libxcb-render0 libxcb-shm0 libxcomposite1 libxcursor1 libxdamage1 \
  libxdmcp6 libxext6 libxfixes3 libxi6 libxrandr2 libxrender1 libxss1 libxtst6 \
  xvfb

RUN mkdir /engine

RUN useradd -ms /bin/bash blimp_user

ADD * /engine/
RUN chown -R blimp_user /engine

USER blimp_user
WORKDIR "/engine"
ENTRYPOINT [ "/usr/bin/xvfb-run", "--server-args=-screen 0 1600x1200x24", \
  "./content_shell", "--no-sandbox" ]
