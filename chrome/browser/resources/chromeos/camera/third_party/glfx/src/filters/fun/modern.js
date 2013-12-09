/**
 * @filter       Modern
 * @description  Colorizes parts of the image with different colors.
 * @param color1 Color of the top-left part.
 * @param color2 Color of the top-right part.
 * @param color3 Color of the bottom-left part.
 * @param color4 Color of the bottom-right part.
 */
function modern(color1, color2, color3, color4) {
    gl.modern = gl.modern || new Shader(null, '\
        uniform sampler2D texture;\
        uniform vec4 color1;\
        uniform vec4 color2;\
        uniform vec4 color3;\
        uniform vec4 color4;\
        varying vec2 texCoord;\
        void main() {\
            vec4 color = texture2D(texture, texCoord);\
            vec4 blendColor;\
            vec4 white = vec4(1, 1, 1, 1);\
            if (texCoord.x < 0.5 && texCoord.y < 0.5)\
              blendColor = color1; else\
            if (texCoord.x >= 0.5 && texCoord.y < 0.5)\
              blendColor = color2; else\
            if (texCoord.x < 0.5 && texCoord.y >= 0.5)\
              blendColor = color3; else\
            if (texCoord.x >= 0.5 && texCoord.y >= 0.5)\
              blendColor = color4;\
            gl_FragColor = color + blendColor * 0.5;\
        }\
    ');

    simpleShader.call(this, gl.modern, {
        color1: color1.concat(1),
        color2: color2.concat(1),
        color3: color3.concat(1),
        color4: color4.concat(1)
    });

    return this;
}
