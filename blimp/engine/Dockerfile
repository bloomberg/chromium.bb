FROM ubuntu:trusty

# Run the command below to update the lib list.
# ldd ./blimp_engine_app | grep usr/lib | awk '{print $3}' | xargs -n1 \
#   dpkg-query -S | awk -F: '{print $1}'
RUN apt-get update && \
  apt-get install -yq libnss3 libnss3 libnss3 libnspr4 libnspr4 libnspr4 \
  libfreetype6 libfontconfig1 libdrm2 libasound2 libcups2 libgssapi-krb5-2 \
  libkrb5-3 libk5crypto3 libstdc++6 libgnutls26 libavahi-common3 \
  libavahi-client3 libkrb5support0 libtasn1-6 libp11-kit0 libffi6

RUN mkdir /engine

RUN useradd -ms /bin/bash blimp_user

ADD * /engine/
RUN mv /engine/chrome_sandbox /engine/chrome-sandbox
RUN chown -R blimp_user /engine

USER blimp_user
WORKDIR "/engine"

ENTRYPOINT ["/engine/blimp_engine_app", "--disable-gpu"]
