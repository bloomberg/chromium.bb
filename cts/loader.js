"use strict";
(async () => {
  const resp = await fetch("cts/listing.json");
  const listing = await resp.json();

  for (const [path, info] of Object.entries(listing)) {
    console.log(path, info);
    if (!path.endsWith("/")) {
      console.log(await import("./out/cts/src/" + path + ".js"));
    }
  }
})();
