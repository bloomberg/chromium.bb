#!/bin/sh
if [ "$REQUEST_METHOD" != "POST" ]; then
  echo Content-type: text/html
  echo
  echo Page must be retrieved with HTTP POST.
  exit 1
fi

cat << EOF
Cache-Control: no-cache
Content-Type: text/plain
Content-Disposition: attachment; filename=superbo.txt
Expires: Fri, 01 Jan 1990 00:00:00 GMT
Transfer-Encoding: chunked

plain text response from a POST
0
EOF
