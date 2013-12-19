/**
 * @filter       Ghost effect
 * @description  Applies a ghost effect.
 */
function ghost() {
    gl.ghost = gl.ghost || new Shader(null, '\
        uniform sampler2D texture;\
        uniform sampler2D ghostTexture;\
        varying vec2 texCoord;\
        void main() {\
            vec4 color = texture2D(texture, texCoord);\
            vec4 ghostColor = texture2D(ghostTexture, texCoord);\
            gl_FragColor = (color * 0.55 + ghostColor * 0.45);\
        }\
    ').textures({ ghostTexture: 2 });

    // Create the buffer textures if not yet crated.
    if (!this._.ghostTextures) {
        this._.ghostTextures = [];
        this._.ghostTextureTimestamps = [];
        this._.ghostFPS = 25;
        this._.ghostDelay = 2000;
        var texturesCount =
            Math.ceil(this._.ghostDelay / 1000 * this._.ghostFPS);
        for (var i = 0; i < texturesCount; i++) {
            this._.ghostTextures[i] = new Texture(
                0, 0, gl.RGBA, gl.UNSIGNED_BYTE);
            this._.ghostTextureTimestamps[i] = null;  // Not used texture.
        }
        this._.ghostLastUsedTexture = 0;
        this._.ghostLastTimestamp = 0;
    }

    // Remember the current frame.
    if (Date.now() - this._.ghostLastTimestamp >=
        this._.ghostDelay / this._.ghostTextures.length) {
        this._.ghostLastTimestamp = Date.now();

        // Find an available texture. Replace the oldest one if not found.
        var currentTextureIndex = null;
        var currentTextureTimestamp = null;
        for (var i = 0; i < this._.ghostTextures.length; i++) {
            var timestamp = this._.ghostTextureTimestamps[i];
            if (timestamp == null) {
                currentTextureIndex = i;
                break;
            }
            if (currentTextureIndex == null ||
                timestamp < currentTextureTimestamp) {
                currentTextureIndex = i;
                currentTextureTimestamp = timestamp;
            }
        }

        // Save the current frame (if there is a texture available).
        var currentTexture = this._.ghostTextures[currentTextureIndex];
        currentTexture.ensureFormat(
            this._.texture.width / 2,
            this._.texture.height / 2,
            this._.texture.format,
            this._.texture.type);
        this._.texture.use();
        currentTexture.drawTo(function() {
            Shader.getDefaultShader().drawRect();
        });
        this._.ghostTextureTimestamps[currentTextureIndex] = Date.now();
    }

    // Restore the ghost frame.
    var ghostTextureTimestamp = Date.now() - this._.ghostDelay;

    // Find the nearest ghost frame.
    var bestIndex = null;
    var bestDistance = null;
    for (var i = 0; i < this._.ghostTextures.length; i++) {
        var timestamp = this._.ghostTextureTimestamps[i];
        if (!timestamp)
            continue;
        var distance = Math.abs(timestamp - ghostTextureTimestamp);
        if (!bestIndex || distance < bestDistance) {
            bestIndex = i;
            bestDistance = distance;
        }
    }

    var ghostTexture = this._.ghostTextures[bestIndex];
    ghostTexture.ensureFormat(
            this._.texture.width / 2,
            this._.texture.height / 2,
            this._.texture.format,
            this._.texture.type);
    ghostTexture.use(2);

    // Apply the shader.
    simpleShader.call(this, gl.ghost, {});
    ghostTexture.unuse();

    return this;
}
